#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

const char go[3] = "GO";

int main(int argc, char* argv[]) {
	int pipes[4][2];
	for(int i=0; i<4; ++i)pipe(pipes[i]);
	
	int score1=0, score2=0;
	
	for(int round = 0; round<10; ++round){
		for(int pl=0; pl<2; ++pl){
			write(pipes[pl][1], go, 3);
		}
		for(int pl=0; pl<2; ++pl){
			pid_t pid = fork();
			if(pid<0){
				perror("fork error");
				exit(-1);
			}
			if(!pid){
				dup2(0, pipes[pl][0]);
				dup2(1, pipes[pl+1][1]);
				char* args[1] = {NULL};
				if(pl==0)execv("player1", args);
				else execv("player2", args);
				exit(-1);
			}
			// wait(NULL);
		}
		char buff[2];
		read(pipes[1][0], buff, 1);
		int move1  = atoi(buff);
		read(pipes[1][0], buff, 1);
		int move2 = atoi(buff);
		if((move1-move2)==1 || (move2-move1)==2)++score1;
		else if((move2-move1)==1 || (move1-move2)==2)++score2;
	}
	fprintf(stderr, "%d %d", score1, score2);
	return 0;
}
