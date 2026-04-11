/*
 * more02.c - moreのバージョン0.2
 * 	ファイルを読み出して24行出力したら、一時停止して、次のコマンドを待つ。
 * 	バージョン0.2の機能：コマンドは/dev/ttyから読み出す。
 */
#include	<stdio.h>
#include	<stdlib.h>

#define PAGELEN 24
#define LINELEN 512

void do_more(FILE *);
int see_more(FILE *);

int main(int ac, char *av[])
{
	FILE	*fp;

	if ( ac == 1 )
		do_more( stdin );
	else
		while ( --ac )
			if ( (fp = fopen( *++av , "r" )) != NULL )
			{
				do_more( fp );
				fclose( fp );
			}
			else
				exit(1);
	return 0;
}

void do_more( FILE *fp )
/*
 * PAGELEN 行の情報を読み、see_more()を呼び出して命令を待つ
 */
{
	char	line[LINELEN];
	int	num_of_lines = 0;
	int	see_more(FILE *), reply;
	FILE	*fp_tty;

	fp_tty = fopen( "/dev/tty", "r" );	/* 新機能：コマンドストリーム	*/
	if ( fp_tty == NULL )				/* オープンに失敗したら	*/
		exit(1);				/* 実行する意味はない	*/

	while ( fgets( line , LINELEN, fp ) ) {		/* 入力がまだある	*/
		if ( num_of_lines == PAGELEN ){		/* 画面がいっぱいに？	*/
			reply = see_more(fp_tty);	/* 新機能：FILE*を渡す	*/
			if ( reply == 0 )		/* n: 終了		*/
				break;
			num_of_lines -= reply;		/* カウンタをリセット	*/
		}
		if ( fputs( line, stdout ) == EOF )	/* 行を表示する		*/
			exit(1);			/* または終了する	*/
		num_of_lines++;				/* 行数を数える		*/
	}
}

int see_more(FILE *cmd)					/* 新機能：引数を受け付ける	*/
/*
 * メッセージを出力して応答を待つ。先に進める行数を返す。
 * qならノー、スペースならイエス、CRなら1行
 */
{
	int	c;

	printf("\033[7m more? \033[m");			/* vt100 では反転表示		*/
	while ( (c = getc(cmd)) != EOF )		/* 新機能：ttyから読み出し	*/
	{
		if ( c == 'q' )
			return 0;
		if ( c == ' ' )
			return PAGELEN;
		if ( c == '\n' )
			return 1;
	}
	return 0;
}
