#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int main(){
    int flagTerm= 0, procCnt = 0, exitStatus = 0, keepGoing = 1, inputFD, outputFD, j , dni, dno,
	stdinRed = 0, stdoutRed = 0; // flags for stdin and out redirection
   
    // Saved file descriptors
	int saveIn = dup(0),  saveOut = dup(1);
	char* inF;
    //Signal handlers
    struct sigaction SIGINT_action_parent;
    SIGINT_action_parent.sa_handler = SIG_IGN; 
    sigfillset(&SIGINT_action_parent.sa_mask);   // initialize and fill signal set http://pubs.opengroup.org/onlinepubs/009695399/functions/sigfillset.html
    sigaction(SIGINT, &SIGINT_action_parent, NULL);

	//Keeps the shell in a loop until the user terminates with exit command
    while (keepGoing == 1) {
		char puts[2049]; //an array to hold input from command line
        // check if any background processes have terminated
		int childExitMethod = -5;
		int bgOrFg = 0;  //bool for either background or foreground commands
		while (procCnt >= 1) { //Check to see if background process is complete
			int childPID = waitpid(-1, &childExitMethod, WNOHANG);
			if (childPID != 0) {
				//Checking Exit Status lecture 
				if (WIFEXITED(childExitMethod) != 0) {
					printf("Background PID %d is done: exit value %d\n", childPID, WEXITSTATUS(childExitMethod));
					fflush(stdout);
				}
				else if (WIFSIGNALED(childExitMethod) != 0) {
					printf("Background PID %d is done: terminated by signal %d\n", childPID, WTERMSIG(childExitMethod));
					fflush(stdout);
				}
				else {
					printf("Background PID %d is done, but did not exit with a signal.\n", childPID);
					fflush(stdout);
				}
				(procCnt)--;
			}
		}
        printf(": "); //colon is used for the command line prompt
        fflush(stdout);  //make sure all text is outputted (don't want a zombie apocalypse).
        fgets(puts, 2048, stdin); //store user input into puts

		// If the user wants to comment using # 
        if (puts[0] == '#') {
            exitStatus = 0;
            continue;
        }

        // If user enters a new line then continue
        if (strcmp(puts, "\n") == 0) {
            exitStatus = 0;
            continue;
        }

        puts[strcspn(puts, "\n")] = 0; //remove new line from puts that comes with user command http://www.cplusplus.com/reference/cstring/strcspn/
        char* cmd = strtok(puts, " "); 

        // if user wishes to exit shell
        if (strcmp(cmd, "exit") == 0) { 
			keepGoing = 0; //ensure shell loop terminates
            kill(0, SIGTERM); //kill all processes
            exit(0);    //exit program
        }

        // for CD functionality
        else if (strcmp(cmd, "cd") == 0) {
            char* tar = strtok(NULL, " "); //the content after the blank space is the directory
			//if no path specified then default to home
            if (tar == NULL) {               
                tar = getenv("HOME"); //http://man7.org/linux/man-pages/man3/getenv.3.html
            }

            if (chdir(tar) != 0) {
                printf("Error changing directory to: %s\n", tar);
                exitStatus = 1;
            }
            else {
                exitStatus = 0;
            }
        }

        // for status functionality 
        else if (strcmp(cmd, "status") == 0) {
            if (flagTerm != 0) {             // if terminated by signal
				printf("terminated by signal %d\n", WTERMSIG(childExitMethod));
            }

            else {    //if it terminates as expected                           
                printf("exit value ");
            }
            printf("%d\n", exitStatus);
            fflush(stdout);
            flagTerm = 0;
            exitStatus = 0;
        }

        //for all other cases not pre-built
        else {

            char* args[512];   // an array to hold arguments
            args[0] = cmd;    
            exitStatus = 0;
             
            int i = 1;
            while (keepGoing == 1) {
                char* arg = strtok(NULL, " ");
                if (arg == NULL) {
                    args[i++] = arg;
                    break;
                }
                // checks for & symbol meaing that it should run in the background.
                else if (strcmp(arg, "&") == 0) {
                    bgOrFg = 1;  //set flag for background process
                }

                // redirecting stdout lecture
                else if (strcmp(arg, ">") == 0) {  
                    char* outfile = strtok(NULL, " ");
                    outputFD = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    if (outputFD == -1) { 
						perror(outfile); 
						exitStatus = 1; 
						break; 
					}
                    saveOut = dup(1); //save stdout
                    // redirecting
                    j = dup2(outputFD, 1);
                    if (j == -1) {
						perror("dup2");
						exit(1); 
					}
                    stdoutRed = 1;
                }

                //redirecting stdin lecture
                else if (strcmp(arg, "<") == 0) {  
                    inF= strtok(NULL, " ");
                    inputFD = open(inF, O_RDONLY);
                    if (inputFD == -1) { 
						perror(inF); 
						exitStatus = 1; 
						break; 
					}
                  
                    saveIn = dup(0); //save stdin
                    j = dup2(inputFD, 0);
                    if (j == -1) {
						perror("dup2");
						exit(1);
					}
                    stdinRed = 1;
                }

                // add to array if not a special character
                else {
                    args[i++] = arg;
                }
            } 
			
            // spawn child, follows lectures closely
            pid_t spawnPid = -5;
            int childExitMethod = -5;
            spawnPid = fork();
            switch(spawnPid) {

                case -1: { //if error spawning
					perror("Error spawning process.\n");
					exit(1); 
					break; 
				}

                case 0: {  
                    // child process signal handler
                    struct sigaction SIGINT_action_child;
                    SIGINT_action_child.sa_handler = SIG_DFL; 
                    sigfillset(&SIGINT_action_child.sa_mask);   
                    sigaction(SIGINT, &SIGINT_action_child, NULL);

                    if (bgOrFg == 1) { //if background flag is set
                        if (stdoutRed == 0) {  // check stdout redirection flag
                            dno = open("/dev/null", O_WRONLY);
                            dup2(dno, 1);
                            stdoutRed = 1;
                        }
                        if (stdinRed== 0) {  // check stdin redirection flag
                            dni= open("/dev/null", O_RDONLY);
                            dup2(dni, 0);
                            stdinRed = 1;
                        }
                    }
                    if (execvp(args[0], args) < 0) { //search for executable in args
                        perror(args[0]);
						printf("execution failure");
                        exit(1); 
                    }
                    break;
                }
                
				default: {  // defaults to parent
                    if (bgOrFg == 0) {  //if background flag is set
                        waitpid(spawnPid, &childExitMethod, 0);
                        exitStatus = WEXITSTATUS(childExitMethod); 
                        // see if foreground process is terminated by signal
                        if (WIFSIGNALED(childExitMethod) != 0) {
                            printf("terminated by signal %d\n", WTERMSIG(childExitMethod));
                            fflush(stdout);
                            exitStatus = WTERMSIG(childExitMethod);
                            flagTerm= 1;   
                        }
                    }
                    //background not set so print PID
                    else {
                        printf("background pid is %d\n", spawnPid);
                        fflush(stdout);
                        procCnt++;
                    }
                    break;
                }
            }
            // restore std in and out.
            dup2(saveOut, 1);
            dup2(saveIn , 0);
            stdinRed= 0;
            stdoutRed = 0;
        }
    }
    return 0;
}