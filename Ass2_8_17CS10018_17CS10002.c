/* Shell OR command-line interpreter program */

#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/wait.h>
#include<fcntl.h>

#define MAX_ARGS 200
#define MAX_CMDS 50
#define MAX_CHAR 1000


// assuming in piping mode, input redirection can be done only for 1st cmd;
// output redirection only for last cmd;
// background run only for last cmd;

int main() {

	char *args[MAX_ARGS];
    char *cmds[MAX_CMDS][MAX_ARGS];
	char cmd[MAX_CHAR];
	char s[MAX_CHAR];
	char *delim=" ",*tok;
    int n_cmd = 0;
	

	while(1) {
		printf("group8$ ");
		memset(cmd, 0, MAX_CHAR);
		fgets(cmd, MAX_CHAR, stdin);
		if(cmd[strlen(cmd)-1] == '\n')cmd[strlen(cmd) - 1] = '\0';
		else cmd[strlen(cmd)] = '\0';

		tok = strtok(cmd,delim);

		int i_in = -1, i_out = -1;  // flags for i/o redirection
		int bck = 0; 				// flag for backgroud run
		char *input_file = NULL, *output_file = NULL;
		int i=0;

        n_cmd = 0;
		while(tok != NULL){
			if(i_in == 1){		// assigning input file name
				input_file = strdup(tok);
				i_in = 0;
			}

			else if(!strcmp(tok, "<")){		// input redirection required
				i_in = 1;
			}

			else {
				if(i_out == 1){			// assigning output file name
					output_file = strdup(tok);
					i_out = 0;
				}

				else if(!strcmp(tok, ">")){		// output redirection required
					i_out = 1;
				}

				else {
					if(!strcmp(tok, "&")) {		// background run
						bck = 1;
					}

					else {
						if(!strcmp(tok, "|")) {         // pipe
                            args[i] = NULL;
                            for(int j=0; j<i ; j++){
                                cmds[n_cmd][j] = strdup(args[j]);
                                args[j] = NULL;
                               
                            }
                            cmds[n_cmd][i]=NULL;
                            i=0;
                            n_cmd++;
                        }
                        
                        else {
                            args[i]=strdup(tok);
						    i++;
                            
                        }
					}
				}
			}
			
			tok=strtok(NULL,delim);
		}
		args[i] = NULL;

        if(n_cmd == 0) {            // without pipes
            switch(fork()){
                case -1:
                    perror("fork failed");
                    exit(EXIT_FAILURE);
                case 0:			/* CHILD */
                    if(i_in != -1){			// input redirection
                        int fd_in;
                        fd_in = open(input_file, O_RDONLY);
                        close(STDIN_FILENO);
                        dup(fd_in);
                    }

                    if(i_out != -1){		// output redirection
                        int fd_out;
                        fd_out = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, S_IWUSR | S_IRUSR);
                        close(STDOUT_FILENO);
                        dup(fd_out);
                    }

                    execvp(args[0], args);
                    exit(0);
                default:		/* PARENT */
                    if(!bck) wait(NULL);
                    fflush(stdout);
            }
        }

        else {          // n_cmd number of pipes
            int fds[n_cmd][2];
            int j;
            for(j=0;j < n_cmd; j++){
                if(pipe(fds[j]) < 0) {
                    perror("pipe creation failed");
                    exit(EXIT_FAILURE);
                }

                switch(fork()){
                    
                    case -1:
                        perror("fork failed");
                        exit(EXIT_FAILURE);
                    case 0:			/* CHILD */
                        if(j == 0){         // 1st cmd
                            if(i_in != -1){			// input redirection
                                int fd_in;
                                fd_in = open(input_file, O_RDONLY);
                                close(STDIN_FILENO);
                                dup(fd_in);
                            }
                            close(fds[j][0]);
                            close(STDOUT_FILENO);
                            dup(fds[j][1]);
                            
                        }
                        else {              // middle cmds
                            close(STDIN_FILENO);
                            dup(fds[j-1][0]);
                            close(fds[j-1][1]);
                            close(STDOUT_FILENO);
                            dup(fds[j][1]);
                            close(fds[j][0]);
                        }

                        execvp(cmds[j][0], cmds[j]);
                        exit(0);
                    default:		/* PARENT */
                        fflush(stdout);
                        
                }
            }
            int p;
            switch(p = fork()){         // last cmd
                    case -1:
                        perror("fork failed");
                        exit(EXIT_FAILURE);
                    case 0:			/* CHILD */
                        close(STDIN_FILENO);
                        dup(fds[j-1][0]);
                        close(fds[j-1][1]);
                        if(i_out != -1){		// output redirection
                            int fd_out;
                            fd_out = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, S_IWUSR | S_IRUSR);
                            close(STDOUT_FILENO);
                            dup(fd_out);
                        }
                       
                        execvp(args[0], args);
                        exit(0);
                    default:		/* PARENT */
                        if(!bck) waitpid(p, NULL, 0);
                        fflush(stdout);
            }
        }

	}
	return 0;
}
