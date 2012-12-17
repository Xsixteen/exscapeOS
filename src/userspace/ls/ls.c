#include <dirent.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <assert.h>

void print_help(void) {
	fprintf(stderr, "Usage: ls [-alh] [files...]\n");
	fprintf(stderr, "-1: display one file per line, with no additional details\n");
	fprintf(stderr, "-a: show hidden files (that begin with a dot)\n");
	fprintf(stderr, "-l: show details, one file per line\n");
	fprintf(stderr, "-F: show a / after directory names\n");
	fprintf(stderr, "-h: display this help message\n");
}

int main(int argc, char **argv) {

	assert(argv[argc] == NULL);

	int c;
	int opt_all = 0, opt_list = 0, opt_singlecol = 0, opt_type = 0;

	while ((c = getopt(argc, argv, "1aFhl")) != -1) {
		switch(c) {
			case 'a':
				opt_all = 1;
				break;
			case 'l':
				opt_list = 1;
				opt_singlecol = 0;
				break;
			case '1': /* overrides -l, if used later on in the command line */
				opt_singlecol = 1;
				opt_list = 0;
				break;
			case 'F':
				opt_type = 1;
				break;
			case 'h':
				// TODO: shouldn't be for help; 'h' is used for human-readable sizes in other ls implementations
				print_help();
				exit(0);
				break;
			default:
				exit(1);
				break;
		}
	}
	argc -= optind;
	argv += optind;

	const char **files = malloc((argc + 2) * sizeof(char *));
	memset(files, 0, (argc + 2) * sizeof(char *));

	if (argc == 0) {
		// no files/directories were passed as arguments
		files[0] = ".";
	}
	else
		memcpy(files, argv, argc * sizeof(char *));

	do {
		if (argc > 1) {
			// TODO: only print this if the argument is a directory
			printf("%s:\n", *files);
		}

		DIR *dir = opendir(*files);
		if (!dir) {
			fprintf(stderr, "ls: ");
			perror(*files);
			continue;
		}

		struct dirent *dent;
		int line_used = 0; // used for standard mode only
		while ((dent = readdir(dir)) != NULL) {
			if (!opt_all && *(dent->d_name) == '.')
				continue;

			char name[1024] = {0};
			strlcpy(name, *files, 1024);
			if (name[strlen(name) - 1] != '/')
				strlcat(name, "/", 1024);
			strlcat(name, dent->d_name, 1024);

			struct stat st;
			if (stat(name, &st) != 0) {
				fprintf(stderr, "ls: ");
				perror(name);
				continue;
			}

			if (opt_singlecol) {
				printf("%s%s\n", dent->d_name, (opt_type && S_ISDIR(st.st_mode)) ? "/" : "");
				continue;
			}
			else if (opt_list) {

				char perm_str[11] = "-rwxrwxrwx";
				if (st.st_mode & 040000)
					perm_str[0] = 'd';

				for (int i=0; i<9; i++) {
					if (!(st.st_mode & (1 << i))) {
						perm_str[9 - i] = '-';
					}
				}

				printf("%s 1 root  root %8u 01 Jan 1970 %s%s\n", perm_str, (uint32)st.st_size, dent->d_name, (opt_type && S_ISDIR(st.st_mode)) ? "/" : "");
			}
			else {
				// standard format
				if (line_used + dent->d_namlen + 4 > 80) {
					printf("\n");
					line_used = 0;
				}
				line_used += printf("%s%s    ", dent->d_name, (opt_type && S_ISDIR(st.st_mode)) ? "/" : "");
			}
		}

		if (argc > 1) {
			// TODO: only print this if the argument is a directory
			printf("\n");
		}
	} while (*(++files));

	if (!opt_list && !opt_singlecol)
		printf("\n");
	else
		fflush(stdout);

	return 0;
}