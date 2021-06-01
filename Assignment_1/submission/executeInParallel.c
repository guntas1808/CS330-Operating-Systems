#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int countWords(char* str, char* delim);
char** splitstr(char* str, char* delim);
int executeCommand(char* cmd);
int execute_in_parallel(char *infile, char *outfile);


int main(int argc, char *argv[])
{
	return execute_in_parallel(argv[1], argv[2]);
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
	word_list[i-1] = strtok(word_list[i-1], "\n");
	word_list[i] = NULL;
	return word_list;
}

int executeCommand (char *cmd) {
		
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
		printf("UNABLE TO EXECUTE\n");

	return 0;
}

int execute_in_parallel(char *infile, char *outfile){
	pid_t pid;
	int in_fd = open(infile, O_RDONLY);
	int out_fd = open(outfile, O_RDWR|O_CREAT, S_IRWXO|S_IRWXU|S_IRWXG);
	dup2(in_fd, 0);
	dup2(out_fd, 1);
	char lines[51][4000];
	int num_lines=0;
	
	while(fgets(lines[num_lines], 4000, stdin))++num_lines;
	
	int pipes[num_lines][2];
	for(int i=0; i<num_lines; ++i){
		pipe(pipes[i]);
	}
	for(int i=0; i<num_lines; ++i){
		pid = fork();
		if(pid<0)perror("fork");
		if(!pid){
			dup2(pipes[i][1], 1);
			executeCommand(lines[i]);
			exit(0);
		}
	}
	char buff[num_lines][4000];
	dup2(in_fd, 0);
	for(int i=0; i<num_lines; ++i){
		read(pipes[i][0], buff[i], 4000);
		fprintf(stderr,"%s", buff[i]);
		printf("%s", buff[i]);
	}
	return 0;
}