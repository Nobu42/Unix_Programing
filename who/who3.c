/*
 * who3.c - 読み出しでバッファリングを行うwho
 *        - 空レコードが出力されないようにし、
 *        - 時刻を適切に整形する
 *        - 入力をバッファリングする( utmplib.c を使う)
*/
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <utmp.h>
#include <fcntl.h>
#include <time.h>
#include "utmplib.h"  // common/utmplib.h を読み込む

// mainの中の個別宣言は不要になります
#define SHOWHOST

void show_info(struct utmp *);
void showtime(time_t);

int main()
{
	struct utmp *utbufp;

        if ( utmp_open( UTMP_FILE ) == -1 ){
               perror(UTMP_FILE);
	       exit(1);
         }
	while ( ( utbufp = utmp_next() ) != ((struct utmp *) NULL) )
		show_info( utbufp );
	utmp_close( );
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

	printf("%-8.8s", utbufp->ut_name);	/* ログイン名	*/
	printf(" ");				/* スペース	*/
	printf("%-8.8s", utbufp->ut_line);	/* tty		*/
	printf(" ");				/* スペース	*/
	showtime( utbufp->ut_time );		/* 時刻の表示	*/
#ifdef SHOWHOST
	if ( utbufp->ut_host[0] != '\0' )
		printf(" (%s)", utbufp->ut_host);	/* ホスト	*/
#endif
	printf("\n");
}
void showtime( long timeval )
/*
 * 	人間が理解しやすい形式で時刻を表示する。
 * 	ctime を使って文字列を組み立ててから、その一部を抜き出す。
 * 	注意：%12.12sは文字列を char 12 個分で出力し、
 * 	長さを１２バイト以下に制限する。
 */
{
	char	*cp;		/* 時刻のアドレスを保持するポインタ 	*/

	cp = ctime(&timeval);	/* 時刻を文字列に変換する		*/
				/* 文字列は次の通り			*/
				/* Mon Feb 4 00:46:40 EST 1991		*/
				/* 0123456789012345.			*/
				/* 位置 4 から 12文字分を抜き出す	*/
	printf("%12.12s", cp+4);
}

