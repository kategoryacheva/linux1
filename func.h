#include "fs_work.h"

void ls() {
	char path[2] = ".";
	char* arg2;
	char* arg3;
	arg2 = strtok(NULL, " \n");
	if (arg2 == NULL) {
		ils(path);
		return;
	} else {
		arg3 = strtok(NULL, " \n");
		if (arg3 == NULL) {
			ils(arg2);
			return;
		}
	}
	printf("use help command to find out about usage\n");
}

void cd() {
	char path[2] = "/";
	char* arg2;
	char* arg3;
	arg2 = strtok(NULL, " \n");
	if (arg2 == NULL) {
		icd(path);
		rewrite_sb();
		return;
	} else {
		arg3 = strtok(NULL, " \n");
		if (arg3 == NULL) {
			if (icd(arg2)) {
				printf("cd: wrong path\n");
			}
			rewrite_sb();
			return;
		}
	}
	printf("use help command to find out about usage\n");
}

void mkdir() {
	char* arg;
	arg = strtok(NULL," \n");
	if (arg == NULL) {
		printf("use help command to find out about usage\n");
	}
	while (arg) {
		if (create_file(arg, DIR)) {
			printf("mkdir: wrong path %s\n", arg);
		}
		rewrite_sb();
		arg = strtok(NULL, " \n");
	}
}

void touch() {
	char* arg;
	arg = strtok(NULL," \n");
	if (arg == NULL) {
		printf("use help command to find out about usage\n");
	}
	while (arg) {
		if (create_file(arg, FIL)) {
			printf("mkdir: wrong path %s\n", arg);
		}
		rewrite_sb();
		arg = strtok(NULL, " \n");
	}
}

void rm() {
	char* arg;
	int error;
	arg = strtok(NULL, " \n");
	if (arg == NULL) {
		printf("use help command to find out about usage\n");
		return;
	}
	if (!strcmp(arg,"-r")) {
		arg = strtok(NULL, " \n");
		while (arg) {
			if (irm(arg, RECURSIVE)) {
				printf("rm: wrong path\n");
			}
			rewrite_sb();
			arg = strtok(NULL, " \n");
		}
	} else {
		while (arg) {
			error = irm(arg, SIMPLE);
			switch (error) {
				case 1:
					printf("rm: wrong path \n");
					break;
				case 2:
					printf("rm: cannot remove directory, try \"rm -r\"\n");
					break;
				default:
					break;
			}
			rewrite_sb();
			arg = strtok(NULL, " \n");
		}
	}
}

void import() {
	char* arg2;
	char* arg3;
	arg2 = strtok(NULL, " \n");
	if(arg2) {
		arg3 = strtok(NULL, " \n");
		if(arg3) {
			if (iimport(arg2, arg3)) {
				printf("import: error\n");
			}
			rewrite_sb();
			return;
		}
	}
	printf("use help command to find out about usage\n");
}

void export() {
	char* arg2;
	char* arg3;
	arg2 = strtok(NULL, " \n");
	if(arg2) {
		arg3 = strtok(NULL, " \n");
		if(arg3) {
			if (iexport(arg2, arg3)) {
				printf("export: error\n");
			}
			return;
		}
	}
	printf("use help command to find out about usage\n");
}
void cat() {
	char* arg;
	arg = strtok(NULL," \n");
	if (arg == NULL) {
		printf("use help command to find out about usage\n");
	}
	while (arg){
		if (icat(arg)){
			printf("cat: wrong path %s\n", arg);
		} else {
			printf("\n");
		}
		arg = strtok(NULL, " \n");
	}
}

void help() {
	printf("list files: ls [$directory$] \n"
		"change directory: cd [$directory$]\n"
		"make directory: mkdir $directory$ (one or more)\n"
		"create file: touch $file$ (one or more)\n"
		"delete files: rm [-r] $files$ (one or more, use [-r] for directories)\n"
		"import file: import $external file name$ $internal file name$\n"
		"export file: export $internal file name$ $external file name$\n"
		"write files to standard output sequentially: cat $file$ (one or more)\n"
		"exit: exit\n"
		"* arguments in [] are optional\n");
}
