#include <curses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "2048.h"

static int score, seed;
static int size, size2;
static int can_insert;
static char *cells;
int board_init(int setseed, int setsize)
{
	score = 0;

	if (!setseed)
		seed = time(0);
	else
		seed = setseed;
	srand(seed);

	size = setsize;
	size2 = size * size;
	can_insert = 2;

	cells = malloc(size2 * 2);
	if (!cells) {
		perror(argv0);
		return 1;
	}
	memset(cells, 0, size2);

	return 0;
}

void board_fini(void)
{
	free(cells);
}

static int newpos;
void board_insert(void)
{
	if (!can_insert)
		return;
	char *map = cells + size2;
	int ins = 0;
	for (int i = 0; i < size2; ++i)
		if (!cells[i])
			map[ins++] = i;
	if (ins) {
		ins = map[rand() % ins];
		cells[ins] = rand() % 10 ? 1 : 2;
		--can_insert;
		newpos = ins;
	}
}

void board_draw(void)
{
	mvprintw(0, 0, "seed:%10d score:%10d", seed, score);
	for (int x = 0; x < size + 1; ++x)
		mvvline(1, x * 8, '|', 4 * size);
	for (int y = 0; y <= size; ++y)
		for (int x = 0; x < size; ++x) {
			mvaddstr(y * 4 + 1, x * 8, "+-------+");
			if (y < size) {
				mvaddstr(y * 4 + 3, x * 8 + 1, "       ");
				int i = y * size + x;
				unsigned int n = cells[i];
				if (n) {
					n = 1 << n;
					if (i == newpos)
						attron(A_REVERSE);
					int len = snprintf(0, 0, "%u", n);
					mvprintw(y * 4 + 3,
							x * 8 + 1 + (7 - len) / 2,
							"%u", n);
					if (i == newpos)
						attroff(A_REVERSE);
				}
			}
		}
	refresh();
}

void board_dump(int fd)
{
	char *buf = malloc(BUFSIZ);
	if (!buf) {
		perror(argv0);
		return;
	}

	int len = snprintf(buf, BUFSIZ, "seed=%d score=%d size=%d", seed, score,
			size);
	for (int i = 0; i < size2; ++i)
		len += snprintf(buf + len, BUFSIZ - len, " %d", cells[i]);
	buf[len++] = '\n';
	if (write(fd, buf, len) < 0)
		perror(argv0);

	free(buf);
}

static void shift(int start, int stop, int step)
{
	for (int i = start + step; i != stop; i += step) {
		char n = cells[i];
		if (n) {
			char s = cells[start];
			if (!s) {
				cells[start] = n;
				cells[i] = 0;
			} else if (n == s) {
				++cells[start];
				score += 1 << cells[start];
				cells[i] = 0;
				start += step;
			} else {
				start += step;
				i = start;
				continue;
			}
			i = start;
			can_insert = 1;
		}
	}
}

void board_up(void)
{
	for (int i = 0; i < size; ++i)
		shift(i, size2 + i, size);
}

void board_down(void)
{
	for (int i = -size; i < 0; ++i)
		shift(size2 + i, i, -size);
}

void board_left(void)
{
	for (int i = 0; i < size2; i += size)
		shift(i, i + size, 1);
}

void board_right(void)
{
	for (int i = -1; i < size2 - 1; i += size)
		shift(i + size, i, -1);
}
