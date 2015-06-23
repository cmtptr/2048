#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "2048.h"

char *client_path(void)
{
	char *path = malloc(FILENAME_MAX);
	if (!path) {
		perror(argv0);
		return 0;
	}
	const char *basename = argv0;
	for (const char *ptr = basename; *ptr;)
		if (*ptr++ == '/')
			basename = ptr;
	int len = snprintf(path, FILENAME_MAX, "/tmp/%s-%d", basename, getuid());
	return realloc(path, len + 1);
}

static size_t cmds_count, cmds_ptr;
static const char **cmds;
void client_addcmd(const char *cmd)
{
	if (cmds_ptr >= cmds_count) {
		cmds_count += 4;
		cmds = realloc(cmds, cmds_count * sizeof *cmds);
	}
	cmds[cmds_ptr++] = cmd;
}

int client_iscmd(void)
{
	return !!cmds_ptr;
}

int client_docmd(void)
{
	char *path = client_path();
	if (!path)
		return 0;

	int fd = socket(PF_UNIX, SOCK_SEQPACKET, 0);
	if (fd < 0) {
		perror(argv0);
		goto abort_path;
	}

	struct sockaddr_un addr = {
		.sun_family = AF_UNIX,
	};
	strncpy(addr.sun_path, path, sizeof addr.sun_path);
	if (connect(fd, (struct sockaddr *)&addr, sizeof addr) < 0) {
		perror(argv0);
		goto abort_fd;
	}

	char *buf = malloc(BUFSIZ);
	if (!buf) {
		perror(argv0);
		goto abort_fd;
	}

	for (size_t i = 0; i < cmds_ptr; ++i) {
		int len = snprintf(buf, BUFSIZ, "%s\n", cmds[i]);
		if (write(fd, buf, len) < 0) {
			perror(argv0);
			goto abort_buf;
		}
		len = read(fd, buf, BUFSIZ);
		if (len < 0) {
			perror(argv0);
			goto abort_buf;
		}
		printf("%.*s", len, buf);
	}

	close(fd);
	free(path);
	free(cmds);
	cmds_count = 0;
	cmds_ptr = 0;
	cmds = 0;
	return 0;

abort_buf:
	free(buf);
abort_fd:
	close(fd);
abort_path:
	free(path);
	return 1;
}
