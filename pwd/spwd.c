/*
 * spwd.c:pwdを単純化したバージョン
 *
 * ※この本アップデートして再発売されないかな。。。内容最高！
 *
 * カレントディレクトリからスタートして再帰的にファイルシステム
 * のルートまで上り、上のディレクトリから順に名前を出力する
 * readdir()を使って個々のディレクトリについての情報を得る
 *
 * 	バグ："/" から実行すると・・・・これも週末なおそ。。
 */
#include	<stdio.h>
#include	<stdlib.h>
#include	<sys/types.h>
#include	<sys/stat.h>
#include	<dirent.h>
#include	<string.h>
#include 	<unistd.h>

ino_t	get_inode(char *);
void	printpathto(ino_t);
void	inum_to_name(ino_t , char *, int );

int
main(int argc, char *argv[])
{
	printpathto( get_inode( "." ) );	/* ここまでのパスを出力する	*/
	putchar('\n');				/* 最後に改行を追加する		*/
	exit(0);
}

void printpathto( ino_t this_inode )
/*
 * this_node に対応するディレクトリまでのパスを出力する
 * 再帰関数
 */
{
	ino_t	my_inode;
	char	its_name[BUFSIZ];

	if ( get_inode("..") != this_inode )
	{
		chdir("..");				/* 一つ上のディレクトリへ	*/

		inum_to_name(this_inode, its_name, BUFSIZ);	/* 名前を取得		*/

		my_inode = get_inode( "." );			/* 再帰だな。。		*/
		printpathto( my_inode );			/* 一つ上のディレクトリ	*/
		printf("/%s", its_name );			/* までを出力		*/
							/* このディレクトリの名前を出力	*/

	}
}

void inum_to_name(ino_t inode_to_find , char *namebuf, int buflen)
/*
 * カレントディレクトリから引数のiノード番号を持つファイル
 * ( サブディレクトリ ) を探し、名前をnamebufにコピーする
 */
{
	DIR	*dir_ptr;		/* ディレクトリ	*/
	struct dirent	*direntp;	/* 各エントリ	*/

	dir_ptr = opendir( "." );
	if ( dir_ptr == NULL ) {
		perror( "." );
		exit(1);
	}

	/*
	 * 指定されたiノード番号を持つファイル（ サブディレクトリ ）を探す
	 */

	while ( ( direntp = readdir( dir_ptr ) ) != NULL )
		if ( direntp->d_ino == inode_to_find )
		{
			strncpy( namebuf, direntp->d_name, buflen);
			namebuf[buflen-1] = '\0';	/* 念の為 */
			closedir( dir_ptr );
			return;
		}
	fprintf(stderr, "error looking for inum %ld\n", inode_to_find);
	exit(1);
}

ino_t get_inode( char *fname )
/*
 * ファイルのiノード番号を返す
 */
{
	struct stat info;

	if ( stat( fname , &info ) == -1 ) {
		fprintf(stderr, "Cannot stat ");
		perror(fname);
		exit(1);
	}
	return info.st_ino;
}

