/*
 * Unix/Linuxプログラミング理論と実践の第2章の課題。
 * fgetsを使いたかったけど、lseekを使えって言ってる。
 * オプションは今度にしよう。。。。
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#define DEFAULT_LINES 10

void display_tail(int fd, int target_count);

int main(int argc, char *argv[]) {
    int fd;

    if (argc != 2) {
        fprintf(stderr, "usage: %s filename\n", argv[0]);
        exit(1);
    }

    if ((fd = open(argv[1], O_RDONLY)) == -1) {
        perror(argv[1]);
        exit(1);
    }

    display_tail(fd, DEFAULT_LINES);

    close(fd);
    return 0;
}

void display_tail(int fd, int target_count) {
    char ch;
    off_t pos;
    int nlines = 0;

    /* ファイルの末尾に移動 */
    pos = lseek(fd, 0, SEEK_END);

    /* ファイルが空、または1バイトのみの場合は終了*/
    if (pos <= 0) return;

    /* 逆方向に1バイトずつスキャン
     * 末尾が改行の場合、その改行はカウントせずに1つ手前から開始する調整が必要な場合がある
     */
    while (pos > 0) {
        pos--;
        lseek(fd, pos, SEEK_SET);

        if (read(fd, &ch, 1) <= 0) break;

        if (ch == '\n') {
            nlines++;
            /* 目的の行数に達したら、その位置が開始地点 */
            if (nlines > target_count) {
                pos++; /* 改行の直後にポインタを合わせる */
                break;
            }
        }
    }

    /* 確定した位置から末尾までを一気に出力 */
    lseek(fd, pos, SEEK_SET);
    char buffer[4096];
    int n_read;
    while ((n_read = read(fd, buffer, sizeof(buffer))) > 0) {
        write(STDOUT_FILENO, buffer, n_read);
    }
}
