#include<stdlib.h>
#include<unistd.h>
#include<stdio.h>
#include<string.h>
#include<wait.h>

int countWords(char* str, char* delim);
char** splitstr(char* str, char* delim);
int executeCommand(char* cmd);

int main (int argc, char *argv[]) {
	return executeCommand(argv[1]);
}

int countWords(char* str, char* delim){
	int count=1;
	while(*str){
		if(*str==*delim)++count;
		++str;
	}
	return count;
}

char** splitstr(char* str, char* delim){
	int count = countWords(str, delim);
	char** word_list = malloc((count+1)*sizeof(char*));
	char* token = strtok(str, delim);
	int i=0;
	while(token){
		word_list[i] = malloc((1+strlen(token))*sizeof(char*));
		strcpy(word_list[i], token);
		++i;
		token = strtok(NULL, delim);
	}
	word_list[i] = NULL;
	return word_list;
}

int executeCommand (char *cmd) {
	pid_t pid;
	int status;
	pid = fork();
	wait(&status);
	
	if(!pid){
		
		char** args = splitstr(cmd, " ");
		
		char** paths = splitstr(getenv("CS330_PATH"), ":");
		
		for(int i=0; i<3; ++i){
			char executable[100];
			strcpy(executable, paths[i]);
			char* exec_path = strcat(executable, "/");
			strcpy(executable, exec_path);
			exec_path = strcat(executable, args[0]);

			execv(exec_path, args);
		}
		exit(-1);
	}else{
		int exit_stat = WEXITSTATUS(status);
		if(exit_stat==0){
			exit(0);
		}else{
			fprintf(stderr ,"UNABLE TO EXECUTE\nEXIT STATUS %d\n", exit_stat);
			exit(exit_stat);
		}
	}
}