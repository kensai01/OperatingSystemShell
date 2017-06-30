#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <sys/wait.h>
#include <stdbool.h>

#include <readline/readline.h>
#include <readline/history.h>

/* Functions that will edit our environment variables & launch internal commands. */
int SetCommand(), DeleteCommand(), PrintCommand(), PwdCommand(), ChangeDirCommand(), QuitCommand();

/*Structure which will contain the programs for the environment manipulation.*/
typedef struct {
    char *name;     //Function name
    int (*func)();  //Replaced the Function call with this, Function is depreciated
    char *doc;      //Tooltip
} COMMAND;

COMMAND Commands[] = {
        {"set", SetCommand, "Set the value of the environment variable."},
        {"delete", DeleteCommand, "Remove the environment variable."},
        {"print", PrintCommand, "Print the value of the named environment variable."},
        {"pwd", PwdCommand, "Print the current working directory." },
        {"cd", ChangeDirCommand, "Change the current working directory to the supplied argument."},
        {"exit", QuitCommand, "Terminate the shell."}
};

/*fwd declarations*/
char *StripWhite(char*);
COMMAND *FindCommand(char*);
char ExecuteCommand(char*);
void Tokenize(char *s, char** arr, char * delimiter);

/*void handler(int signal) {
    // call kill function
    // call printf function
}*/

/*Using quit flag to give us a chance to clean up after ourselves. Otherwise
 * prog would quit right from the quit function and free(s) would never get to run again.*/
int quitFlag = 0;
/* Flags for the external commands. */
int flagDot = 0;
int flagSlash = 0;
int flagInput = 0;
int flagOutput = 0;
int flagPipeInst = 0;


int main(int argc, char **argv) {

    //signal(SIGINT, handler);
    char *s, *line;
    while ((s=readline("prompt> "))){
        if(quitFlag == 0) {
            line = StripWhite(s);
            add_history(line);
            ExecuteCommand(line);
            /* clean up! */
            free(s);
        }
        if(quitFlag == 1){
            exit(0);
        }
    }
}

/*Executes a command. */
char ExecuteCommand(line) char *line;
{
    /*Set up needed variables*/
    register int EachLetter;
    COMMAND *Command;
    char *WholeWord;
    // Holds external command prefix
    char externalCmdPrefix = line[0];

    // output file token
    char * OutFileToken;
    // input file token
    char * InputFileToken;

    // Initial line for tokenization
    char *sentence;
    // Second line for tokenization
    char *sentence1;
    // Third line for tokenization
    char *sentence2;

    /*Allocate space and copy over all the commands.*/
    sentence = malloc(255*sizeof(char));
    strcpy(sentence, line);
    sentence1 = malloc(255*sizeof(char));
    strcpy(sentence1, line);
    sentence2 = malloc(255*sizeof(char));
    strcpy(sentence1, line);

    /*Set the flag according to which external command first element user supplied, either '.' or '/' */
    if (externalCmdPrefix == '.') { flagDot = 1; printf("Dot Flag Set\n");}
    if (externalCmdPrefix == '/') { flagSlash = 1; printf("Slash Flag Set\n");}

    /*Isolate the internal command word from the rest of the message.
     * Used for internal commands. */
    EachLetter = 0;
    while (line[EachLetter] && whitespace(line[EachLetter])) EachLetter++;
    WholeWord = line + EachLetter;
    while (line[EachLetter] && !whitespace(line[EachLetter])) EachLetter++;
    if (line[EachLetter]) line[EachLetter++] = '\0';

    /*Try to find the internal command word in the available list internal commands.*/
    Command = FindCommand(WholeWord);

    /*If the command supplied is NOT internal, do the external commands logic.*/
    if(!Command)
    {
        /*Storage Arrays that will hold our broken up tokenized commands.*/
        char **StorageSimpleExternalCmdArray = malloc(128 * sizeof(char*));
        char **StorageInArr = malloc(128 * sizeof(char*));
        char **StorageOutArr = malloc(128 * sizeof(char*));
        char **StorageParentArgs = malloc(128 * sizeof(char*));
        char **StoragePipelined = malloc(128 * sizeof(char*));
        char **StoragePipeChild = malloc(128 * sizeof(char*));
        char **StoragePipeWholeChildCmd = malloc(128 * sizeof(char*));

        /*Ensure memory was allocated.*/
        if (!StoragePipelined |
                !StorageSimpleExternalCmdArray |
                !StorageInArr |
                !StorageOutArr |
                !StorageParentArgs)
        {
            fprintf(stderr, "Dynamic Memory Allocation Error.\n");
            return 1;
        }
        /*Initial tokenization based on space.*/
        Tokenize(sentence, StorageSimpleExternalCmdArray, " ");
        /*Runs if it's simple pipelining of normal external cmd, ie:
         * ls -al | sort  */
        if (flagPipeInst == 1 && flagInput == 0 && flagOutput == 0) {
            /*Grab the arguments & command for child pipe.*/
            Tokenize(sentence1, StoragePipeChild, "|");
            Tokenize(StoragePipeChild[0], StorageParentArgs, " ");
            Tokenize(StoragePipeChild[1], StoragePipeWholeChildCmd, " ");

            /* What's left should be what is before the pipe line |.*/
            /*Tokenize one more time, arguments to pass to execvp.*/
            //Tokenize(sentence1, StorageParentArgs, " ");
        }
        /*Runs if we found i/o redirection and piping. ie:
         * ls -al < infile | sort > outfile */
        if (flagPipeInst == 1 && flagInput == 1 && flagOutput == 1){

            /*Do another tokenization to seperate the things we need for o redirection.*/
            Tokenize(sentence1, StoragePipelined, ">");

            /*Grab the last element, should be the output file.*/
            int ArrSize = sizeof(**StoragePipelined);
            OutFileToken = StoragePipelined[ArrSize];
            OutFileToken = StripWhite(OutFileToken);

            /*Grab the arguments & command for child pipe.*/
            Tokenize(sentence1, StoragePipeChild, "|");
            Tokenize(StoragePipeChild[1], StoragePipeChild, " ");

            /*Grab the entire child pipe (everything after the |)*/
            Tokenize(sentence2, StoragePipeWholeChildCmd, "|");
            Tokenize(StoragePipeWholeChildCmd[1],StoragePipeWholeChildCmd, " ");

            /*Tokenize again to get the input file.*/
            Tokenize(sentence1, StorageInArr, "<");

            /*Grab the last element, should be the input file.*/
            ArrSize = sizeof(**StorageInArr);
            InputFileToken = StorageInArr[ArrSize];
            InputFileToken = StripWhite(InputFileToken);

            /*Tokenize one more time, what's left should be the arguments to pass to execvp.*/
            Tokenize(sentence1, StorageParentArgs, " ");

        }
        /*Runs if we found both input & output markers during first tokenization.*/
        /* ie: sort < infile > outfile */
        if (flagInput == 1 && flagOutput == 1 && flagPipeInst == 0){
            /*Do another tokenization to seperate the things we need for i/o redirection.*/
            Tokenize(sentence1, StorageOutArr, ">");
            /*Grab the last element, should be the output file.*/
            int ArrSize = sizeof(**StorageOutArr);
            OutFileToken = StorageOutArr[ArrSize];
            OutFileToken = StripWhite(OutFileToken);
            /*Tokenize again to get the input file.*/
            Tokenize(sentence1, StorageInArr, "<");
            /*Grab the last element, should be the input file.*/
            ArrSize = sizeof(**StorageInArr);
            InputFileToken = StorageInArr[ArrSize];
            InputFileToken = StripWhite(InputFileToken);
            /*Tokenize one more time, what's left should be the arguments to pass to execvp.*/
            Tokenize(sentence1, StorageParentArgs, " ");
        }
        /*If we only found input marker.*/
        /*ie: sort < foo*/
        if (flagInput == 1 && flagOutput == 0){
            Tokenize(sentence1, StorageInArr, "<");
            int ArrSize = sizeof(**StorageInArr);
            InputFileToken = StorageInArr[ArrSize];
            InputFileToken = StripWhite(InputFileToken);
            Tokenize(sentence1, StorageParentArgs, " ");
        }
        /*If we only found output marker.*/
        /* ls > save for example..*/
        if (flagInput == 0 && flagOutput == 1){
            Tokenize(sentence1, StorageOutArr, ">");
            int ArrSize = sizeof(**StorageOutArr);
            OutFileToken = StorageOutArr[ArrSize];
            OutFileToken = StripWhite(OutFileToken);
            Tokenize(sentence1, StorageParentArgs, " ");
        }


        /*If we're here, we're doing external commands - so we need a child process.
         * Create Child Process:*/
        pid_t pid;
        pid_t pid2;
        int pipefd[2];
        /*The command is external, so check if its prefixes with a / or a ., if so
        * the user is specifying a complete path to the executable file. Also
         * checks if we have input/output redirection.*/
        if (flagSlash == 1 || flagDot == 1 || flagInput == 1 || flagOutput == 1) {
            pid = fork();
            if (pid < 0){
                printf("ERROR: %s\n", strerror(errno));
                return 1;
                }
                /*No I/O Redirection, supplying complete path starting with . or / */
            else if (pid == 0 && (flagSlash == 1 || flagDot == 1)) {

                if((execvp(StorageSimpleExternalCmdArray[0],StorageSimpleExternalCmdArray)) == -1)
                {
                    printf("ERROR: %s\n", strerror(errno));
                }
                return 1;
            }

            /*I/O Redirection logic.*/
            else if (pid == 0 && (flagInput == 1 && flagOutput == 1) && (flagPipeInst == 0)){
                int InFile, OutFile;
                /*Open Input & Output files.*/
                if ((InFile = open(InputFileToken, O_RDONLY)) == -1){
                    printf("ERROR: %s\n", strerror(errno));
                    return 1;
                }
                if ((OutFile = open(OutFileToken,
                                    O_WRONLY |
                                    O_TRUNC |
                                    O_CREAT,
                                    S_IRUSR |
                                    S_IRGRP |
                                    S_IWGRP |
                                    S_IWUSR)) == -1){
                    printf("ERROR: %s\n", strerror(errno));
                    return 1;
                }
                /*Replace file descriptors /w dup2.*/
                if ((dup2(InFile, STDIN_FILENO)) == -1){
                    printf("ERROR: %s\n", strerror(errno));
                    return 1;
                }
                if ((dup2(OutFile, STDOUT_FILENO)) == -1){
                    printf("ERROR: %s\n", strerror(errno));
                    return 1;
                }
                /*Close open files.*/
                close(InFile);
                close(OutFile);

                if ((execvp(StorageParentArgs[0],StorageParentArgs)) == -1){
                    printf("ERROR: %s\n", strerror(errno));
                    return 1;
                }
            }
            else if (pid == 0 && (flagInput == 1 && flagOutput == 0) && (flagPipeInst == 0)){
                int InFile;
                /*Open Input & Output files.*/
                if ((InFile = open(InputFileToken, O_RDONLY)) == -1){
                    printf("ERROR: %s\n", strerror(errno));
                    return 1;
                }
                /*Replace file descriptors /w dup2.*/
                if ((dup2(InFile, STDIN_FILENO)) == -1){
                    printf("ERROR: %s\n", strerror(errno));
                    return 1;
                }
                /*Close open files.*/
                close(InFile);

                if ((execvp(StorageParentArgs[0],StorageParentArgs)) == -1){
                    printf("ERROR: %s\n", strerror(errno));
                    return 1;
                }
            }
            else if (pid == 0 && (flagInput == 0 && flagOutput == 1) && (flagPipeInst == 0)){
                int OutFile;
                /*Open Input & Output files.*/
                if ((OutFile = open(OutFileToken,
                                    O_WRONLY |
                                    O_TRUNC |
                                    O_CREAT,
                                    S_IRUSR |
                                    S_IRGRP |
                                    S_IWGRP |
                                    S_IWUSR)) == -1){
                    printf("ERROR: %s\n", strerror(errno));
                    return 1;
                }
                /*Replace file descriptors /w dup2.*/
                if ((dup2(OutFile, STDOUT_FILENO)) == -1){
                    printf("ERROR: %s\n", strerror(errno));
                    return 1;
                }
                /*Close open files.*/
                close(OutFile);

                if ((execvp(StorageParentArgs[0],StorageParentArgs)) == -1){
                    printf("ERROR: %s\n", strerror(errno));
                    return 1;
                }
            }

            else {
                //printf("My PID is %d\n", getpid());
                //printf("Local PID %d\n", pid);
                int cs;
                wait(&cs);
                printf("Child Process Complete. Status %d\n", cs);

                /*Reset the i/o markers*/
                flagInput = 0;
                flagOutput = 0;
                /*Reset External Command markers.*/
                flagDot = 0;
                flagSlash = 0;
                flagPipeInst = 0;
                return 0;
            }
        }
        if (flagPipeInst == 1){
            pipe(pipefd);

            if ((pid = fork()) == -1) {
                fprintf(stderr, "Fork Failed!\n");
                return 1;
            }

            if ((pid2 = fork()) == -1) {
                fprintf(stderr, "Fork Failed!\n");
                return 1;
            }

            if (pid == 0 && pid2 > 0){
                printf("Child-1");
                dup2(pipefd[0], STDIN_FILENO);
                close(pipefd[1]);
                execvp(StoragePipeChild[0], StoragePipeWholeChildCmd);
            }
            int status;
            while(waitpid(pid, &status, WUNTRACED | WCONTINUED) > 0);


            if (pid2 == 0 && pid > 0){
                printf("Child-2");
                dup2(pipefd[1], STDOUT_FILENO);
                close(pipefd[0]);
                execvp(StorageParentArgs[0], StorageParentArgs);
            }
            flagPipeInst = 0;

            while (wait(&status) > 0)

                if(pid2 >0 && pid>0){
                    printf("\n");
                }

                //printf("My PID is %d\n", getpid());
                //printf("Local PID %d\n", pid);
                //int cs;
                wait(&status);
                printf("Child Process Complete. Status %d\n", status);
                flagPipeInst = 0;
                return 0;
        }
            /*Simple external command that is not I/O redirect & not a . or / prefixed command.
             * ie. ls -al or sort ...*/
        else {
            pid = fork();
            if (pid < 0){
                printf("Fork Failed\n");
                return 1;
            }
            else if (pid == 0) {
                //printf("My PID is %d\n", getpid());
                //printf("Executable Name Supplied.\n");
                //printf("Local PID %d\n", pid);;
                /*User is supplying the executable file name.*/
                execvp(StorageSimpleExternalCmdArray[0],StorageSimpleExternalCmdArray);
            }

            else {
                //printf("My PID is %d\n", getpid());
                //printf("Local PID %d\n", pid);
                int cs;
                wait(&cs);
                printf("Child Process Complete. Status %d\n", cs);
                return 0;
            }
        }
    }

    while (whitespace (line[EachLetter])) EachLetter++;
    WholeWord = line + EachLetter;
    return ((*(Command->func)) (WholeWord));
}



/*Look up command name, return null ptr if none was found otherwise
 * return the pointer to the command.*/
COMMAND * FindCommand(Name) char *Name; {
        register int CommandNum;

        for (CommandNum = 0; Commands[CommandNum].name; CommandNum++) {
            if (strcmp(Name, Commands[CommandNum].name) == 0) return (&Commands[CommandNum]);
        }
        return ((COMMAND *) NULL);

}

/*Strip white space
 * Returns a pointer to a string.*/
char * StripWhite(string) char *string; {
    register char *s, *t;
    for(s = string; whitespace (*s); s++);
    if (*s == 0) return (s);
    t = s + strlen(s) - 1;
    while(t>s && whitespace(*t)) t--;
    *++t = '\0';
    return s;
}

/* Set the value of the environment variable to the value specified.*/
int SetCommand(arg) char *arg;{
    //local variables to parse the incoming character array
    char *tmp,*tmp1;

    /*Tokenize the char array to delimit at the equals sign*/
    /*Effectively getting the name of the environment variable to set*/
    tmp = strtok(arg, "=");
    /*Remove any remaining white space*/
    tmp = StripWhite(tmp);
    /*Print result for visual debug*/
    printf("EVarName to be set: %s\n", tmp);
    /*Tokenize the rest of the character array until the terminal character*/
    /*Effectively get the environment variable value to set*/
    tmp1 = strtok(NULL, "\0");
    /*Remove any remaining white space*/
    tmp1 = StripWhite(tmp1);
    /*Print result for visual debug*/
    printf("EVarVal to be set: %s\n", tmp1);
    /*Set environment variable based on supplied parameters.*/
    if (setenv(tmp,tmp1,1) == 0)
    {
        printf("Environment variable set.\n");
        return 0;
    }
    else
        printf("Unable to set Environment Variable. Please try again.\n");
    return 0;
}

/* Delete the named environment variable. */
int DeleteCommand(arg) char *arg;{
    char *x;
    x = getenv(arg);
    printf("Environment Variable to be Deleted: %s\n", arg);
    printf("Environment Variable Value before Deletion: %s\n", x);
    if(unsetenv(arg) == 0) {
        x = getenv(arg);
        printf("Environment Variable Value after Deletion Attempt: %s\n", (x != NULL) ? x : "undefined");
        return 0;
    }
    else
        printf("Unable to Delete Environment Variable. Please Try Again.\n");
    return 1;
}

/*Print the named environment variable. */
int PrintCommand(arg) char *arg;{
    char *x;
    x = getenv(arg);
    printf("Environment Variable Name: %s\n", arg);
    printf("Environment Variable Value: %s\n", (x != NULL) ? x : "undefined");
    return 1;
}

int PwdCommand(arg) char *arg; {
    char Directory[2048], *DirectoryString;
    DirectoryString = getcwd(Directory, 2048);
    if (DirectoryString == 0){
        printf("error");
    }
    printf("Current Directory: %s\n",Directory);
    return 0;
}

int ChangeDirCommand(arg) char *arg; {
    /*Ensure that we don't get an error back when changing directories.*/
    if (chdir(arg) == -1){
        perror(arg);
        return 1;
    }
        /* Otherwise get the new directory and print it.*/
    else
        PwdCommand("");
        return 0;
}

/*Sets the quit flag on so that the program can clean up and exit.*/
int QuitCommand(arg) char *arg; {
    /*Set the quit flag on.*/
    quitFlag = 1;
    return 0;
}

void Tokenize(char *sentence, char** StorageArray, char * delimiter)
{
    char *token;
    char *next_token;
    int counter = 1, position = 0;
    token = strtok(sentence, delimiter);
    next_token = token;
    while (next_token != NULL){
        printf("Token %d: %s\n", counter, token);
        StorageArray[position] = token;

        /*Check for input / output markers*/
        if(strcmp(StorageArray[position], "<") == 0){ flagInput = 1; printf("< was located!.\n");}
        if(strcmp(StorageArray[position], ">") == 0){ flagOutput = 1; printf("> was located!.\n");}

        /*Check for pipeline instructions.*/
        if(strcmp(StorageArray[position], "|") == 0){ flagPipeInst = 1; printf("| was located!.\n");}

        position++;
        if((next_token = strtok(NULL, delimiter))){
            token = next_token;
        }
        counter++;
    }
    counter--;
    //printf("Token %d: %s\n", counter, token);
    //printf("StorageArray: %s\n", StorageArray[0]);
    //printf("StorageArray: %s\n", StorageArray[1]);
    //printf("StorageArray: %s\n", StorageArray[2]);
}




