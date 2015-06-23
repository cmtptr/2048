#include <curses.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>

#include "2048.h"

static const char *const usage_short = "\
Try '%s --help' for more information.\n\
";

static const char *const usage_long = "\
usage: %s [<option>...]\n\
Play 2048.\n\
\n\
options:\n\
  -c <cmd>, --command=<cmd>  Connect as a client and send <cmd> to the\n\
                             automation interface (can be given multiple times\n\
                             to send multiple commands).\n\
  -h, --help                 Display this text.\n\
  -r <n>, --seed=<n>         Use <n> to seed the pseudo-random number generator.\n\
  -s <n>, --size=<n>         Play with a board of <n>x<n> size (default 4).\n\
";

const char *argv0;

int main(int argc, char **argv)
{
	argv0 = argv[0];
	int size = 4, seed = 0;

	while (1) {
		static struct option options[] = {
			{"command", required_argument, 0, 'c'},
			{"help", no_argument, 0, 'h'},
			{"seed", required_argument, 0, 'r'},
			{"size", required_argument, 0, 's'},
			{0}
		};
		int flag = getopt_long(argc, argv, "c:hr:s:", options, 0);
		if (flag < 0)
			break;
		switch (flag) {
		case 'c':
			client_addcmd(optarg);
			break;
		case 'h':
			printf(usage_long, argv0);
			return 0;
		case 'r':
			seed = atoi(optarg);
			if (seed <= 0) {
				fprintf(stderr, "%s: invalid seed=\"%s\"\n",
						argv0, optarg);
				return 1;
			}
			break;
		case 's':
			size = atoi(optarg);
			if (size < 2) {
				fprintf(stderr, "%s: invalid size=\"%s\"\n",
						argv0, optarg);
				fprintf(stderr, usage_short, argv0);
				return 1;
			}
			break;
		default:
			fprintf(stderr, usage_short, argv0);
			return 1;
		}
	}

	if (client_iscmd())
		return client_docmd();

	if (board_init(seed, size))
		return 1;

	if (event_init()) {
		board_fini();
		return 1;
	}

	if (!initscr()) {
		fprintf(stderr, "%s: failed to initialized curses\n", argv0);
		event_fini();
		board_fini();
		return 1;
	}
	cbreak();
	curs_set(0);
	keypad(stdscr, TRUE);
	noecho();
	nodelay(stdscr, TRUE);

	int status = -1;
	board_insert();
	do {
		board_insert();
		board_draw();
	} while (status = event_process(), status < 0);

	endwin();
	event_fini();
	board_fini();
	return status;
}
