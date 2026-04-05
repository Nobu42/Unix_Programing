/*
 * who2.c	- /etc/utmp を読み出し、その中の情報のリストを表示する
 * 		- 空レコードが出力されないようにし、自国を適切に整形する
 */
#include	<stdio.h>
#include	<stdlib.h>
#include	<unistd.h>
#include	<utmp.h>
#include	<fcntl.h>
#include	<time.h>

/* #define	SHOWHOST */

void showtime(long);
void show_info(struct utmp *);

int main(void)
{
	struct utmp	utbuf;		/* 情報をここに読み出す */
	int		utmpfd;		/* このデスクリプタから読み出す */

	if ( (utmpfd = open(UTMP_FILE, O_RDONLY)) == -1) {
		perror(UTMP_FILE);
		exit(1);
	}

	while ( read(utmpfd, &utbuf, sizeof(utbuf)) == sizeof(utbuf) )
		show_info( &utbuf );
	close(utmpfd);
	return 0;
}
/*
 * 	show_info()
 * 		utmp 構造体の内容を人間が読めるように表示する
 * 		レコードにユーザー名が含まれていない場合には何も表示しない
 */
void show_info( struct utmp *utbufp )
{
	if ( utbufp->ut_type != USER_PROCESS )
		return;

	printf("%-8.8s", utbufp->ut_name);		/* ログイン名	*/
	printf(" ");					/* スペース	*/
	printf("%-8.8s", utbufp->ut_line);		/* tty		*/
	printf(" ");					/* スペース	*/
	showtime( utbufp->ut_time );			/* 時刻の表示	*/
#ifdef SHOWHOST
	if ( utbufp->ut_host[0] != '\0' )
		printf(" (%s)", utbufp->ut_host);	/* ホスト	*/
#endif
	printf("\n");
}
void showtime( long timeval )
/*
 *	人間が理解しやすい形式で自国を表示する。
 *	ctime を使って文字列を組み立ててから、その一部を抜き出す。
 *	注意：%12.12sは文字列を char 12 個分で出力し、
 *	長さを12バイト以下に制限する。
 */
{
	char	*cp;			/* 時刻のアドレスを保持するポインタ 	*/

	cp = ctime(&timeval);		/* 時刻を文字列に変換する 		*/
					/* 文字列は次の通り			*/
					/* Mon Feb 4 00:46:40 EST 1991		*/
					/* 0123456789012345.			*/
	printf("%12.12s", cp+4);	/* 位置4から12字文を抜き出す		*/
}
