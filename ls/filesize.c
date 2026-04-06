/*
 * filesize.c - passwd ファイルのサイズを出力する
 */
#include	<stdio.h>
#include	<stdlib.h>
#include	<sys/stat.h>

int main(void)
{
	struct stat infobuf;				/* 情報を格納する場所	*/

	if ( stat( "/etc/passwd", &infobuf) == -1 )	/* 情報を取得する	*/
		perror("/etc/passwd");
	else
		printf(" the size of /etc/passwd is %ld\n", infobuf.st_size );

	return EXIT_SUCCESS;
}
