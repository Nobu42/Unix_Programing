#include	<stdio.h>
#include	<sys/types.h>
#include	<sys/stat.h>

void show_stat_info(char *fname, struct stat *buf);

int main(int ac, char *av[])
{
	struct stat info;		/* ファイル情報のためのバッファ */

	if (ac > 1) {
		if ( stat(av[1], &info ) != -1 ) {
			show_stat_info( av[1], &info );
			return 0;
		}
		else
			perror(av[1]);	/* stat()エラーを報告する */
	}
	return 1;
}

void show_stat_info(char *fname, struct stat *buf)
/*
 * 名前=値形式でstatの情報の一部を表示する
 */
{
	printf("   mode: %o\n", buf->st_mode);
	printf("  links: %ld\n", buf->st_nlink);
	printf("   user: %d\n", buf->st_uid);
	printf("  group: %d\n", buf->st_gid);
	printf("   size: %ld\n", buf->st_size);
	printf("modtime: %ld\n", buf->st_mtime);
	printf("   name: %s\n", fname);
}
