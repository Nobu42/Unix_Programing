/* utmplib.h - utmplib.c のインターフェース定義 */

#ifndef _UTMPLIB_H_
#define _UTMPLIB_H_

#include <utmp.h>

/* 関数のプロトタイプ宣言（案内板） */
int utmp_open( char *filename );
struct utmp *utmp_next();
void utmp_close();

#endif
