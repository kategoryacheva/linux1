#include <stdio.h>
#include "func.h"

int main(int argc, char* argv[]){
 	char command[1024];
	char* name;
	if (argc > 2){
		printf("usage: ./myFs [-i] \n");
	return 1;
	}
	if (argc == 2 && !strcmp(argv[1],"-i")){
		myFs_install();
	} else {
		if (argc == 2) {
			printf("wrong arguments, try ./myFs or ./myFs -i to install new fs\n");
			return 1;
		}
	}
	if((myFs = fopen(MYFS_FILENAME, "r+b")) == NULL) {
		printf("Cannot open myFs file, maybe fs is not installed\n");
		return 1;
	}
	while(1){
		printf("myFS> ");
		if(!fgets(command, 1024, stdin)){
			printf("can't read command\n");
			break;
		}
		name = strtok(command," \n");
		read_sb();
		if (!check_sb()){
			printf("FS is not installed\n");
		}
		read_group_desc_table();
		if (!name) continue;
		if (!strcmp(name,"ls")) {
			ls();
		} else if (!strcmp(name,"cd")) {
			cd();
		} else if (!strcmp(name,"mkdir")) {
			mkdir();
		} else if (!strcmp(name,"touch")) {
			touch();
		} else if (!strcmp(name,"import")) {
			import();
		} else if (!strcmp(name,"export")) {
			export();
		} else if (!strcmp(name,"rm")) {
			rm();
		} else if (!strcmp(name,"cat")) {
			cat();
		} else if (!strcmp(name, "help")) {
			help();
		} else if (!strcmp(name,"exit")) {
			break;
		} else {
			printf("no such command\n");
		}
	}
	fclose(myFs);
	return 0;
}
