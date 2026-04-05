/*
 * who01.c - who プログラムの最初のバージョン。
 * 		utmp ファイルをオープンし、読み出した結果を表示する
 */
#include	<stdio.h>
#include	<stdlib.h>
#include	<utmp.h>
#include	<fcntl.h>
#include	<unistd.h>

#define SHOWHOST	/* 出力にリモートマシン名を組み込む */

void show_info( struct utmp *utbufp );

int main(void)
{
	struct utmp	current_record;	/* 情報をここに読み出す		*/
	int		utmpfd;		/* このでスクリプタから読み出す	*/
	int		reclen = sizeof(current_record);

	if ( (utmpfd = open(UTMP_FILE, O_RDONLY)) == -1) {
		perror( UTMP_FILE );	/* UTMP_FILE は utmp.h で定義されている */
		exit(1);
	}
	while ( read(utmpfd, &current_record, reclen) == reclen)
		show_info(&current_record);
	close(utmpfd);
	return 0;
}

/*
 * show info()
 * 	utmp 構造体の内容を人間が読めるように表示する
 * 	*注意* サイズ情報をハードコードしてはならない
 */
void show_info( struct utmp *utbufp )
{
	printf("%-8.8s", utbufp->ut_name);	/* ログイン名 	*/
	printf(" ");				/* スペース	*/
	printf("%-8.8s", utbufp->ut_line);	/* tty		*/
	printf(" ");				/* スペース	*/
	printf("%10d", utbufp->ut_time);	/* ログイン時刻 */
	printf(" ");				/* スペース	*/
#ifdef	SHOWHOST
	printf("(%s)", utbufp->ut_host);	/* ホスト	*/
#endif
	printf("\n");
}
