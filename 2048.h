#ifndef TWENTY_FORTY_EIGHT_H
#define TWENTY_FORTY_EIGHT_H

extern const char *argv0;

char *client_path(void);
void client_addcmd(const char *cmd);
int client_iscmd(void);
int client_docmd(void);

int board_init(int seed, int size);
void board_fini(void);
void board_insert(void);
void board_draw(void);
void board_dump(int fd);
void board_up(void);
void board_down(void);
void board_left(void);
void board_right(void);

int event_init(void);
void event_fini(void);
int event_process(void);

#endif
