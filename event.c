#include <curses.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "2048.h"

static struct event_handler {
	struct event_handler **prev, *next;
	int fd;
	int (*in)(struct event_handler *);
	int (*err)(struct event_handler *);
	int (*hup)(struct event_handler *);
} *handlers;

struct client_handler {
	struct event_handler eh;
	char buf[7], *ptr;  /* enough for up to "right\n\0" */
	size_t count;
};

static int epfd;
static int add_event_handler(struct event_handler *handler)
{
	if (epoll_ctl(epfd, EPOLL_CTL_ADD, handler->fd, &(struct epoll_event){
		.events = EPOLLIN | EPOLLRDHUP,
		.data = {.ptr = handler},
	}) < 0) {
		perror(argv0);
		return -errno;
	}
	handler->prev = &handlers;
	handler->next = handlers;
	if (handlers)
		handlers->prev = &handler->next;
	handlers = handler;
	return 0;
}

static void remove_event_handler(struct event_handler *handler)
{
	*handler->prev = handler->next;
	if (handler->next)
		handler->next->prev = handler->prev;
	if (epoll_ctl(epfd, EPOLL_CTL_DEL, handler->fd, 0) < 0)
		perror(argv0);
}

static int event_handler_stdin_in(struct event_handler *handler)
{
	(void)handler;
	switch (getch()) {
	case 'h':
	case KEY_LEFT:
		board_left();
		break;
	case KEY_DOWN:
	case 'j':
		board_down();
		break;
	case KEY_UP:
	case 'k':
		board_up();
		break;
	case KEY_RIGHT:
	case 'l':
		board_right();
		break;
	case 'q':
		return 0;
	}
	return -1;
}

static int event_handler_stdin_hup(struct event_handler *handler)
{
	(void)handler;
	return 0;
}

static int event_handler_client_hup(struct event_handler *client)
{
	remove_event_handler(client);
	close(client->fd);
	free(client);
	return -1;
}

static void event_handler_client_in_down(int fd)
{
	write(fd, "\0", 1);
	board_down();
}

static void event_handler_client_in_left(int fd)
{
	write(fd, "\0", 1);
	board_left();
}

static void event_handler_client_in_right(int fd)
{
	write(fd, "\0", 1);
	board_right();
}

static void event_handler_client_in_up(int fd)
{
	write(fd, "\0", 1);
	board_up();
}

static int event_handler_client_in(struct event_handler *eh)
{
	struct client_handler *client = (struct client_handler *)eh;

	ssize_t n;
	while (n = read(eh->fd, client->ptr, client->count), n > 0) {
		client->buf[n] = '\0';
		char *end = strchr(client->ptr, '\n');
		if (!end) {
			if ((size_t)n == client->count) {
				client->ptr = client->buf;
				client->count = sizeof client->buf - 1;
			} else {
				client->ptr += n;
				client->count -= n;
			}
			continue;
		}
		*end++ = '\0';

		static const struct {
			const char *cmd;
			void (*act)(int);
		} commands[] = {
			{"down", event_handler_client_in_down},
			{"dump", board_dump},
			{"left", event_handler_client_in_left},
			{"right", event_handler_client_in_right},
			{"up", event_handler_client_in_up},
		};
		size_t i = 0;
		for (; i < sizeof commands / sizeof *commands; ++i)
			if (!strncmp(client->buf, commands[i].cmd,
						sizeof client->buf)) {
				commands[i].act(eh->fd);
				break;
			}
		if (i >= sizeof commands / sizeof *commands) {
			char buf[20 + sizeof client->buf];
			write(eh->fd, buf, snprintf(buf, sizeof buf,
						"invalid command: \"%s\"\n",
						client->buf));
		}

		size_t len = client->ptr + n - end;
		memcpy(client->buf, end, len);
		client->ptr = client->buf;
		client->count = sizeof client->buf - 1 - len;
	}

	if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
		perror(argv0);
		return event_handler_client_hup(eh);
	}

	return -1;
}

static int event_handler_listen_in(struct event_handler *listen)
{
	int fd = accept(listen->fd, 0, 0);
	if (fd < 0) {
		perror(argv0);
		close(fd);
		return 1;
	}

	int flags = fcntl(fd, F_GETFL, 0);
	if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
		perror(argv0);
		close(fd);
		return 1;
	}

	struct client_handler *client = malloc(sizeof *client);
	if (!client) {
		perror(argv0);
		close(fd);
		return 1;
	}

	*client = (struct client_handler){
		.eh = {
			.fd = fd,
			.in = event_handler_client_in,
			.err = event_handler_client_hup,
			.hup = event_handler_client_hup,
		},
		.ptr = client->buf,
		.count = sizeof client->buf - 1,
	};

	if (add_event_handler(&client->eh)) {
		free(client);
		close(fd);
		return 1;
	}

	return -1;
}

static int event_handler_listen_hup(struct event_handler *listen)
{
	fprintf(stderr, "%s: listening socket %d closed\n", argv0, listen->fd);
	return 1;
}

static char *path;
int event_init(void)
{
	epfd = epoll_create1(0);
	if (epfd < 0) {
		perror(argv0);
		return 1;
	}

	static struct event_handler handler = {
		.in = event_handler_stdin_in,
		.hup = event_handler_stdin_hup,
		.err = event_handler_stdin_hup,
	};
	if (epoll_ctl(epfd, EPOLL_CTL_ADD, STDIN_FILENO, &(struct epoll_event){
		.events = EPOLLIN | EPOLLRDHUP,
		.data = {.ptr = &handler},
	}) < 0) {
		perror(argv0);
		goto abort_epfd;
	}

	char *path = client_path();
	if (!path)
		goto abort_epfd;

	if (unlink(path) < 0 && errno != ENOENT) {
		perror(path);
		goto abort_path;
	}

	int fd = socket(PF_UNIX, SOCK_SEQPACKET, 0);
	if (fd < 0) {
		perror(argv0);
		goto abort_path;
	}

	struct sockaddr_un addr = {
		.sun_family = AF_UNIX,
	};
	strncpy(addr.sun_path, path, sizeof addr.sun_path);
	if (bind(fd, (struct sockaddr *)&addr, sizeof addr) < 0
			|| listen(fd, 0) < 0) {
		perror(argv0);
		goto abort_fd;
	}

	struct event_handler *listen = malloc(sizeof *listen);
	if (!listen) {
		perror(argv0);
		goto abort_fd;
	}

	*listen = (struct event_handler){
		.prev = &handlers,
		.next = handlers,
		.fd = fd,
		.in = event_handler_listen_in,
		.hup = event_handler_listen_hup,
		.err = event_handler_listen_hup,
	};

	if (add_event_handler(listen))
		goto abort_listen;

	return 0;

abort_listen:
	free(listen);
abort_fd:
	close(fd);
abort_path:
	free(path);
abort_epfd:
	close(epfd);
	return 1;
}

void event_fini(void)
{
	unlink(path);
	free(path);
	while (handlers) {
		struct event_handler *next = handlers->next;
		close(handlers->fd);
		free(handlers);
		handlers = next;
	}
	close(epfd);
}

int event_process(void)
{
	struct epoll_event events[8];
	int count = epoll_wait(epfd, events, sizeof events / sizeof *events, -1);
	if (count < 0 && errno != EINTR) {
		perror(argv0);
		return 1;
	};

	int status = -1;
	for (int i = 0; i < count; ++i) {
		struct event_handler *handler = events[i].data.ptr;
		if (events[i].events & EPOLLERR) {
			status = handler->err(handler);
			if (status >= 0)
				break;
		}
		if (events[i].events & EPOLLIN) {
			status = handler->in(handler);
			if (status >= 0)
				break;
		}
		if (events[i].events & (EPOLLHUP | EPOLLRDHUP)) {
			status = handler->hup(handler);
			if (status >= 0)
				break;
		}
	}

	return status;
}
