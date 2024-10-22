
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>

pid_t backgroundProcesses[512];
int backgroundProcessCount = 0;
int backgroundEnabled = 1;
pid_t foregroundProcess = -1;
int childExitStatus = -5;       //saves exit status of the last process

//struct that holds all information about a command
struct Command {
    char* commandName;
    char* arguments[512];
    char* inputRedirect;
    char* outputRedirect;
    int runInBackground; //1-background, 0-foreground
};

//executes cd command
void execCd(struct Command* command){
    if(command->arguments[1] == NULL){
        char* homeDir = getenv("HOME");
        chdir(homeDir);
    } else {
        char* newDir = command->arguments[1];
        if(newDir[0] == '/'){
            //absolute path
            chdir(newDir);
        } else {
            //relative path
            char currentDir[1024];
            getcwd(currentDir, sizeof(currentDir));
            char newPath[2048];
            snprintf(newPath, sizeof(newPath),"%s/%s", currentDir, newDir); //concatenate curernt directory with new directory
            chdir(newPath);
        }
    }
}

//makes background processes ingore sigint
void setupBackgroundSigintHandler() {
    struct sigaction bgSIGINT_action = {0};
    bgSIGINT_action.sa_handler = SIG_IGN; // Ignore SIGINT for background processes
    sigfillset(&bgSIGINT_action.sa_mask);
    bgSIGINT_action.sa_flags = 0;

    // Set up the custom signal handler for background processes
    sigaction(SIGINT, &bgSIGINT_action, NULL);
}

//checks the status of all background processes and prints it
void checkBackgroundProcesses(){
    for (int i = 0; i < backgroundProcessCount; i++) {
        pid_t pid = backgroundProcesses[i];
        int status;

        pid_t result = waitpid(pid, &status, WNOHANG);  // Check the status of the background process without blocking

        if(result == 0){
            printf("background pid is %d\n", pid);
            fflush(stdout);
        } else if (result > 0){
            if(WIFEXITED(status) != 0){
                childExitStatus = WEXITSTATUS(status);
                printf("background pid %d is done: exit value %d\n", pid, childExitStatus);
                fflush(stdout);
            } else {
                childExitStatus = WTERMSIG(status);
                printf("background pid %d is done: terminates by signal %d\n", pid, childExitStatus);
                fflush(stdout);
            }

            // Remove the completed process from the backgroundProcesses array
            for (int j = i; j < backgroundProcessCount - 1; j++) {
                backgroundProcesses[j] = backgroundProcesses[j + 1];
            }
            backgroundProcessCount--;
        }
    }
}

//handles non built in commands
void execNonBuiltIns(struct Command* command){
    //printf("exec non built in\n");
    //fflush(stdout);
    pid_t spawnPid = -5;
    
    spawnPid = fork();
    switch(spawnPid){
        case -1: {
            perror("Error executing command\n");
            exit(1);
        }
        case 0: {
            if(command->runInBackground == 1){
                setupBackgroundSigintHandler(); //if background command, turn off sigint
            }

            //change input file for the child process
            if(command->inputRedirect){
                int inputFile = open(command->inputRedirect, O_RDONLY);
                if (inputFile == -1) {
                    perror(command->inputRedirect);
                    exit(1);
                }
                dup2(inputFile, 0);
                close(inputFile);
            }

            //change output file for the child process
            if(command->outputRedirect){
                int outputFile = open(command->outputRedirect, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (outputFile == -1) {
                    perror(command->outputRedirect);
                    exit(1);
                }
                dup2(outputFile, 1);
                close(outputFile);
            }

            // If execvp fails, print an error message and exit with non-zero status
            execvp(command->commandName, command->arguments);
            perror(command->commandName);
            // Use _exit to immediately terminate the child process after execvp failure
            exit(1);
        } default: {
            if((command->runInBackground == 1) && (backgroundEnabled == 1)){
                backgroundProcesses[backgroundProcessCount] = spawnPid;
                backgroundProcessCount++;
            } else {
                foregroundProcess = spawnPid;
                pid_t actualPid = waitpid(spawnPid, &childExitStatus, 0);
            }
        }
    }
}

//if the command is built in then executes it, otherwise passes it to execNonBuiltIns
void execBuiltIns(struct Command* command){
    if(strcmp(command->arguments[0], "status") == 0){
        //print either the exit status or the terminating signal of the last foreground process
        if(childExitStatus != -5){ 
            if (WIFEXITED(childExitStatus)) {
                printf("exit value %d\n", WEXITSTATUS(childExitStatus));
                fflush(stdout);
            } else if (WIFSIGNALED(childExitStatus)) {
                printf("terminated by signal %d\n", WTERMSIG(childExitStatus));
                fflush(stdout);
            }
        } else {
            printf("exit value 0\n");
            fflush(stdout);
        }

    } else if (strcmp(command->arguments[0], "cd") == 0){
        execCd(command);
    } else {
        execNonBuiltIns(command);
    }
}

//initializes command struct
void initializeCommand(struct Command* command){
    command->commandName = NULL;
    for(int i = 0; i < 512; i++){
        command->arguments[i] = NULL;
    }
    command->inputRedirect = NULL;
    command->outputRedirect = NULL;
    command->runInBackground = 0;
}

//proccese a command and puts all info in a command struct for further functions
void processCommand(char input[]){
    struct Command* command = malloc(sizeof(struct Command));
    initializeCommand(command);
    char* saveptr;
    int argIndex = 0;

    char* token = strtok_r(input, " ", &saveptr);
    while(token){
        if(strcmp(token, "<") == 0){
            token = strtok_r(NULL, " ", &saveptr); //get input file
            if(token){
                command->inputRedirect = calloc(strlen(token)+1, sizeof(char));
                strcpy(command->inputRedirect, token);
            }
        } else if (strcmp(token, ">") == 0){
            token = strtok_r(NULL, " ", &saveptr); //get output file
            if(token){
                command->outputRedirect = calloc(strlen(token)+1, sizeof(char));
                strcpy(command->outputRedirect, token);
            }
        } else if (strcmp(token, "&") == 0){
            command->runInBackground = 1;   //set run in background flag
        } else {
            command->arguments[argIndex] = calloc(strlen(token)+1, sizeof(char));
            strcpy(command->arguments[argIndex], token);
            argIndex++;
        }

        token = strtok_r(NULL, " ", &saveptr);  //get next token
    }

    //the command name is the first argument
    if(argIndex > 0){
        command->commandName = calloc(strlen(command->arguments[0])+1, sizeof(char));
        strcpy(command->commandName, command->arguments[0]);
    }

    execBuiltIns(command);  //check if the command is the built-in one
}

//handles $$ variable expansion
void variableExpansion(char input[]){
    int notAllExpanded = 1;
    char modifiedInput[2048];

    while(notAllExpanded){ //keep replacing until no $$ left in the string
        char* variablePlace = strstr(input, "$$");
        if(variablePlace){
            char pretext[2048];
            memset(pretext, '\0', strlen(pretext));
            strncpy(pretext, input, variablePlace - input);
            char posttext[2048];
            memset(posttext,'\0', strlen(posttext));

            char* posttextStart = variablePlace + 2;  // Skip "$$"
            char* posttextEnd = strchr(posttextStart, '\0');  // Find the end of the string
            strncpy(posttext, posttextStart, posttextEnd - posttextStart);

            snprintf(modifiedInput, sizeof(modifiedInput), "%s%d%s", pretext, getpid(), posttext);
            strcpy(input, modifiedInput);
        } else {
            notAllExpanded = 0;
        }
    }
    processCommand(input); //pass the input to this function after variable expansion
}

//checks input for exit command or empty line
int proccessInput(char input[]){
    char inputCopy[strlen(input) + 1];  // +1 for the null terminator
    strcpy(inputCopy, input);           //copy to preserve the input

    char* saveptr; 
    char* token = strtok_r(inputCopy, " ", &saveptr);

    if(!token){
        return 1; //empty string, reprompt
    } else if (strcmp(token, "exit") == 0){
        return 0; //return 0, terminates the program in main
    } else {
        //do $$ expansion
        variableExpansion(input);
        return 1;
    }
}

//handles sigtstp
void sigtstpHandler(int signo) {
    if (backgroundEnabled) {
        printf("Entering foreground-only mode (& is now ignored)\n");
        fflush(stdout);
        backgroundEnabled = 0; // Disable background execution
    } else {
        printf("Exiting foreground-only mode\n");
        fflush(stdout);
        backgroundEnabled = 1; // Enable background execution
    }
}

//handles sigint
//terminates foreground processes, the parent (shell) ignores
void sigintHandler(int signo){
    if (foregroundProcess > 0) {
        if(signo == SIGINT){
            int status;
            pid_t result = waitpid(foregroundProcess, &status, WNOHANG);

            //printf("result %d\n",result);
            if (result == 0) {
                // Process is still running, send SIGINT signal
                kill(foregroundProcess, SIGINT);
                waitpid(foregroundProcess, &status, 0);
                printf("terminated by signal %d\n", signo);
                fflush(stdout);
            } else {
                // Process has already terminated
                printf("\n"); // Print a new line for better formatting
                foregroundProcess = -1;
                fflush(stdout);
            }
        }
    } else {
        printf("\n"); // Print a new line for better formatting
        fflush(stdout);
    }
}

// Reap zombie processes
void sigchldHandler(int signo) {
    while (waitpid(-1, NULL, WNOHANG) > 0) {
        // Empty loop to reap all zombie processes
    }
}

int main(int argc, char *argv[]){
    struct sigaction SIGINT_action = {0}, SIGTSTP_action = {0}, SIGCHLD_action = {0};

    SIGINT_action.sa_handler = sigintHandler;
    sigfillset(&SIGINT_action.sa_mask);
    SIGINT_action.sa_flags = 0;

    SIGTSTP_action.sa_handler = sigtstpHandler;
    sigfillset(&SIGTSTP_action.sa_mask);
    SIGTSTP_action.sa_flags = 0;

    //register signal handlers
    sigaction(SIGINT, &SIGINT_action, NULL);
    sigaction(SIGTSTP, &SIGTSTP_action, NULL);
    sigaction(SIGCHLD, &SIGCHLD_action, NULL);

    char buffer[2048];
    memset(buffer, '\0', strlen(buffer));
    int notExit = 1;

    while(notExit){
        checkBackgroundProcesses();

        //printf("%d", foregroundProcess);
        printf(": ");
        fflush(stdout);
        fgets(buffer, sizeof(buffer), stdin);
        size_t length = strlen(buffer);

        if (length > 0 && buffer[length-1] == '\n'){
            buffer[length - 1] = '\0';
        }

        notExit = proccessInput(buffer);
    }
    return 0;
}