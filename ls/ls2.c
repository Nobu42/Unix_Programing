/*
 * ls2.c
 * 	目的  ディレクトリ（　複数の場合も含む ）の内容をリストアップする
 * 	動作　引数がない場合には"."、そうでなければ引数のディレクトリに
 * 	　　　含まれるファイルを出力する
 * 	注意　stat と pwd.h、grp.h を使う
 * 	バグ　ls2 /tmp を試す
 */
#include	<stdio.h>
#include	<stdlib.h>
#include	<dirent.h>
#include	<sys/stat.h>
#include	<string.h>

void do_ls(char[]);
void dostat(char *);
void show_file_info( char *, struct stat *);
void mode_to_letters( int , char [] );
char *uid_to_name( uid_t );
char *gid_to_name( gid_t );

int main(int ac, char *av[])
{
	if ( ac == 1 )
		do_ls( "." );
	else
		while ( --ac ) {
			printf("%s:\n", *++av );
			do_ls( *av );
		}
	exit(0);
}

void do_ls( char dirname[] )
/*
 * dirname というディレクトリのファイルをリスアップする
 */
{
	DIR	*dir_ptr;
	struct dirent	*direntp;

	if ( ( dir_ptr = opendir( dirname ) ) == NULL )
		fprintf(stderr, "ls2: cannat open %s\n", dirname);
	else
	{
		while ( ( direntp = readdir( dir_ptr ) ) != NULL )
			dostat( direntp-> d_name );
		closedir(dir_ptr);
	}
}

void dostat( char *filename )
{
	struct stat info;

	if ( stat(filename, &info) == -1 )	/* stat 失敗		*/
		perror( filename );		/* エラーの理由を示す	*/
	else					/* そうでなければ情報を表示	*/
		show_file_info( filename, &info );
}

void show_file_info( char *filename, struct stat *info_p )
/*
 * 'fileinfo'についての情報を表示する。
 * 情報は、*info_pが指す構造体に格納されている
 */
{
	char	*uid_to_name(), *ctime(), *gid_to_name(), *filemode();
	void	mode_to_letters();
	char	modestr[11];

	mode_to_letters( info_p->st_mode, modestr );

	printf( "%s"     , modestr );
	printf( "%4d "   , (int) info_p->st_nlink);
	printf( "%-8s "  , uid_to_name(info_p->st_uid) );
	printf( "%-8s "  , gid_to_name(info_p->st_gid) );
	printf( "%8ld "  , (long)info_p->st_size);
	printf( "%.12s " , 4+ctime(&info_p->st_mtime));
	printf( "%s\n"   , filename );
}

/*
 * ユーティリティ関数
 */

/* この関数はst_modeとchar配列を受け付け、この配列にファイルタイプ
 * とパーミッションに対応する9文字をセットする。
 * 注意：suid, sgid, stickyビットに関連するコードはセットしない
 */
void mode_to_letters( int mode, char str[] )
{
	strcpy( str, "----------" );			/* デフォルト = パーミッションなし */

	if ( S_ISDIR(mode) )	str[0] = 'd';		/* ディレクトリ?		*/
	if ( S_ISCHR(mode) )	str[0] = 'c';		/* キャラクタデバイス		*/
	if ( S_ISBLK(mode) )	str[0] = 'b';		/* ブロックデバイス		*/

	if ( mode & S_IRUSR )	str[1] = 'r';		/* ユーザーのパーミッションを	*/
	if ( mode & S_IWUSR )	str[2] = 'w';		/* 表す3ビット			*/
	if ( mode & S_IXUSR )	str[3] = 'x';

	if ( mode & S_IRGRP )	str[4] = 'r';		/* グループのパーミッションを	*/
	if ( mode & S_IWGRP )	str[5] = 'w';		/* 表す3ビット			*/
	if ( mode & S_IXGRP )	str[6] = 'x';

	if ( mode & S_IROTH )	str[7] = 'r';		/* そのほかのユーザーの		*/
	if ( mode & S_IWOTH )	str[8] = 'w';		/* パーミッションを表す３ビット	*/
	if ( mode % S_IXOTH )	str[9] = 'x';
}

#include	<pwd.h>

char *uid_to_name( uid_t uid )
/*
 * uidに対応するユーザー名を指すポインタを返す。getpwuid(3)を使う
 */
{
	struct passwd *getpwuid(), *pw_ptr;
	static char numstr[10];

	if ( ( pw_ptr = getpwuid( uid ) ) == NULL ) {
		sprintf(numstr, "%d", uid);
		return numstr;
	}
	else
		return pw_ptr->pw_name ;
}

#include	<grp.h>

char *gid_to_name( gid_t gid )
/*
 * gidに対応するグループ名を指すポインタを返す。getgrgid(3) を使う
 */
{
	struct group *getgrgid(), *grp_ptr;
	static char numstr[10];

	if ( ( grp_ptr = getgrgid(gid) ) == NULL ) {
		sprintf(numstr, "%d", gid);
		return numstr;
	}
	else
		return grp_ptr->gr_name;
}
