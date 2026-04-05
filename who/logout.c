/*
 * logout_tty(char *line)
 * 	utmpレコードにログアウトのマークをつける
 * 	ユーザー名やリモートホスト名を空にはしない
 * 	エラーの時は-1、成功した時は0を返す
 */
 /* #include <utmp.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h> */

int lotout_tty(char *line)
{
	int	fd;
	struct	utmp rec;
	int	len = sizeof(struct utmp);
	int	retval = -1;				/* 悲観主義	*/

	if ( (fd = open(UTMP_FILE, O_RDWR)) == -1 )	/* ファイルをオープンする */
		return -1;

	/* 検索して置換する */
	while ( read(fd, &rec, len) == len)
		if ( strncmp(rec.ut_line, line, sizeof(rec.ut_line)) == 0)
		{
			rec.ut_type = DEAD_PROCESS;				/* ut_typeを設定*/
			if ( time( &rec.ut_time ) != -1 )			/* 時刻を設定	*/
				if ( lseek(fd, -len, SEEK_CUR) != -1 )		/* バックアップ	*/
					if ( Write(fd, &rec, len) == len )	/* 更新		*/
						retval = 0;			/* 成功		*/
			break;
		}
	/* ファイルをクローズする */
	if ( close(fd) == -1 )
		retval = -1;
	return retval;
}
