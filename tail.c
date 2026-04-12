#include 	<stdio.h>
#include	<stdlib.h>
#include	<fcntl.h>
#include	<unistd.h>

#define DEFAULT_LINES 10

void display_tail(int fd, int target_count);

int main(int ac, char *av[])
{
	int 	fd;

	/* ファイルをしているかチェック */
	if (ac != 2) {
		fprintf(stderr, "usage: %s filename \n", av[0]);
		exit(1);
	}

	/* ファイルが無事に開ける？ */
	if ((fd = open(av[1], O_RDONLY)) == -1) {
		perror(av[1]);
		exit(1);
	}

	display_tail(fd, DEFAULT_LINES);

	close(fd);
	return 0;

}

void display_tail(int fd, int target_count)
{
	char	ch;
	off_t	pos;
	int 	nlines = 0;

	/* ファイルの末尾に移動 */
	pos = lseek(fd, 0, SEEK_END);

	/* ファイルが空、または１バイトのみの場合は終了 */
}
