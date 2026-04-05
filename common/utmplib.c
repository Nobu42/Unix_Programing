/*
 * utmplib.c - utmp ファイルからの読み出しをバッファリングする関数
 *
 * 	含まれる関数は次のとおり
 * 		utmp_open( filename )	- ファイルをオープンする
 * 			エラーの時には-1を返す
 * 		utmp_next( )
 * 			eof の時にはNULL を返す
 * 		utmp_close()		- ファイルを閉じる
 *
 * 	一度の読み出しでNRECS個の構造体を読み出し、バッファから構造体を一個ずつ提供する
 */
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>      // O_RDONLY のために必要
#include <unistd.h>     // read, close, open のために必要
#include <sys/types.h>
#include <utmp.h>
#include "utmplib.h"    // 追加

#define NRECS 16
#define NULLUT ((struct utmp *)NULL)
#define UTSIZE (sizeof(struct utmp))

static	char	utmpbuf[NRECS * UTSIZE];		/* メモリ領域		*/
static	int	num_recs;				/* 格納してある個数	*/
static	int	cur_rec;				/* 次のレコード番号	*/
static	int	fd_utmp = -1;				/* 読み出し元		*/

int utmp_reload()
/*
 * 		バッファに、次のレコードのチャンクを読み出す
 */
{
	int	amt_read;
					/* 読み出したバイト数			*/
	amt_read = read( fd_utmp , utmpbuf, NRECS * UTSIZE );
					/* 何個得られたか？			*/
	num_recs = amt_read/UTSIZE;
					/* ポインタをリセットする		*/
	cur_rec = 0;
	return num_recs;
}

int utmp_open( char *filename )
{
	fd_utmp = open( filename, O_RDONLY );		/* オープンする 	*/
	cur_rec = num_recs = 0;				/* まだレコードはない	*/
	return fd_utmp;					/* 結果を報告		*/
}

struct utmp *utmp_next()
{
	struct utmp *recp;

	if ( fd_utmp == -1 )				/* エラー？		*/
		return NULLUT;
	if ( cur_rec==num_recs && utmp_reload()== 0 )	/* まだ残っている?	*/
		return NULL;
				/* 次のレコードのアドレスを取得する		*/
	recp = ( struct utmp * ) &utmpbuf[cur_rec * UTSIZE];
	cur_rec++;
	return recp;
}

void utmp_close()
{
	if ( fd_utmp != -1 )		/* オープンされていなければ		*/
		close( fd_utmp );	/* クローズしない			*/
}
