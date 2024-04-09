#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h> 
#include <dirent.h>
#include <glob.h>
#include <fcntl.h>

int modeInteractive = 0; //indicates interactive or batch mode
int QUIT = 0; //indicate when to exit
char path[500]; //path to external programs for external exe or which cmd
char wildcardArgs[500]; //wildcard expansion
int lengthofArgs = -1; //Find the amount of arguments
int redirectPos = 0;

void checkCMDS(char* cmdLine, char** args, int cmdPos);
int checkForRWP(char** args, int cmdPos);

char *readLine(){ //NEED TO CHANGE UP
	char *line = (char *)malloc(sizeof(char) * 1024); // Dynamically Allocate Buffer
	char c;
	int pos = 0, bufsize = 1024;
	if (!line) // Buffer Allocation Failed
	{
		printf("\nBuffer Allocation Error.");
		exit(EXIT_FAILURE);
	}
	while(1)
	{
		c=getchar();
		if (c == EOF || c == '\n') // If End of File or New line, replace with Null character
		{
			line[pos] = '\0';
			return line;
		}
		else
		{
			line[pos] = c;
		}
		pos ++;
		// If we have exceeded the buffer
		if (pos >= bufsize)
		{
			bufsize += 1024;
			line = realloc(line, sizeof(char) * bufsize);
			if (!line) // Buffer Allocation Failed
			{
			printf("\nBuffer Allocation Error.");
			exit(EXIT_FAILURE);
			}
		}
	}
}   

char **splitLine(char *cmdLine){ //NEED TO CHANGE UP
	char **tokens = (char **)malloc(sizeof(char *) * 64);
	char *token;
	char delim[10] = " \t\n\r\a";
	int pos = 0, bufsize = 64;
	if (!tokens)
	{
		printf("\nBuffer Allocation Error.");
		exit(EXIT_FAILURE);
	}
	token = strtok(cmdLine, delim);
	while (token != NULL)
	{
		tokens[pos] = token;
		pos ++;
		if (pos >= bufsize)
		{
			bufsize += 64;
			cmdLine = realloc(cmdLine, bufsize * sizeof(char *));
			if (!cmdLine) // Buffer Allocation Failed
			{
			printf("\nBuffer Allocation Error.");
			exit(EXIT_FAILURE);
			}
		}
		token = strtok(NULL, delim);
	}
	tokens[pos] = NULL;

    lengthofArgs = -1;
    while (tokens[++lengthofArgs] != NULL){}

	return tokens;
}

void exeExternalCMDS(char **args) { //executes external CMDS, change some more
    pid_t pid, wpid;
    int childExitStatus, status;
    pid = fork();
    if (pid == 0) {
        // Child process
        if (wildcardArgs[0] != '\0'){
            status = execv(path, splitLine(wildcardArgs));
            if (status == -1) {
                printf("Error executing program\n");
            }
        exit(EXIT_FAILURE);
        }
        else{
            status = execv(path, args);
            if (status == -1) {
                printf("Error executing program\n");
        }
        exit(EXIT_FAILURE);
        }
    } 
    else if (pid < 0) {
        printf("Error forking\n");
    } 
    else {
        do {
            wpid = waitpid(pid, &childExitStatus, WUNTRACED);
        } while (!WIFEXITED(childExitStatus) && !WIFSIGNALED(childExitStatus));
    }
    wildcardArgs[0] = '\0'; 
}
void redirectInput(char** args, int inputPos);
void redirectOutput(char** args, int outputPos);
int copyFilePath(char** args, int whichCallee, int pos){ //helper function for checkExternalCMDS()
    int len = strlen(path);
    path[len] = '/';
    int oldLen = len+1;
    len = strlen(args[pos]) + 1;
    strncpy(&path[oldLen], args[pos], len);
    if (access(path, F_OK) == 0){
        if (whichCallee){
        }
        else{
            int RWP = checkForRWP(args, pos);
            if (RWP == 1){
                //pipe
                return 1;
            }
            if (RWP == 2){
                //redirect >
                redirectOutput(args, redirectPos);
                return 1;
            }
            if (RWP == 3){
                //redirect <
                redirectInput(args, redirectPos);
                return 1;
            }
            exeExternalCMDS(args);
        }
        return 1;
    }
    return 0;
}

int checkExternalCMDS(char **args, int whichCallee){ //Checks for external CMDS
    int pos = 0, checker = 0;
    if (whichCallee){
        pos = 1;
    }
    if (access(args[pos], F_OK) == 0){
        }
        switch (1){
            case 1: 
                strcpy(path, "/usr/local/bin");
                checker = copyFilePath(args, whichCallee, pos);
                if (checker){
                    break;
                }
            case 2:
                strcpy(path, "/usr/bin");
                checker = copyFilePath(args, whichCallee, pos);
                if (checker){
                    break;
                }
            case 3:
                strcpy(path, "/bin");
                checker = copyFilePath(args, whichCallee, pos);
                if (checker){
                    break;
                }
        }
    return checker;
}

//Declarations of Builtin/Internal Shell Commands
void cdShell(char** args, int cmdPos);
void pwdShell(char** args);
void whichShell(char** args, int cmdPos);

int checkInternalCMDS(char** args, int cmdPos){ //checks for Internal CMDS
    if (!strcmp(args[cmdPos], "quit")){
        QUIT = 1;
        if (modeInteractive){
            printf("Exiting my Shell\n");
        }
        return 1;
    }
    else if (!strcmp(args[cmdPos], "cd")){
        cdShell(args, cmdPos);
        return 1;
    }
    else if (!strcmp(args[cmdPos], "pwd")){
        pwdShell(args);
        return 1;
    }
    else if (!strcmp(args[cmdPos], "which")){
        whichShell(args, cmdPos);
        return 1;
    }
    return 0;
}

void cdShell(char** args, int cmdPos){ //cd CMD
    if (args[cmdPos+1] != NULL){
        if (chdir(args[cmdPos+1])){
            printf("mysh: %s: %s: No such file or directory\n", args[cmdPos], args[cmdPos+1]);
        }
    }
}

void pwdShell(char** args){ //pwd CMD
    char pwd[512];
    getcwd(pwd, sizeof(pwd));
    printf("%s\n", pwd);
}

void whichShell(char** args, int cmdPos){ //which CMD
    if (checkExternalCMDS(args, 1) && args[cmdPos+1] != NULL){
        printf("%s\n", path);
    }
    path[0] = '\0';
}

void wildcard(char* wCard){ //wildcard helper function
    int r;
    glob_t gstruct;
    r = glob(wCard, GLOB_ERR, NULL, &gstruct);
    if (r!=0){
        printf("No Matches\n");
        return;
    }
    char** found;
    found = gstruct.gl_pathv;
    strcpy(wildcardArgs, wCard);
    while(*found){
        strcat(wildcardArgs, " ");
        strcat(wildcardArgs, *found);
        found++;
    }
    strcpy(wCard, wildcardArgs);
    globfree(&gstruct);
}

void redirectInput(char** args, int inputPos){
}
void redirectOutput(char** args, int outputPos){
}

void testPipe(char** args, int cmdPos);

int checkForRWP(char** args, int cmdPos){ //checks for redirects, wildcards, and pipes
    for(int i = cmdPos; i < lengthofArgs; i++){
        if (strchr(args[i], '*')){ //wildcard
            wildcard(args[i]);
        }
        if (!strcmp(args[i], "|")){ //pipe
            printf("Found |\n");
            testPipe(args, i);
            return 1;
            //checkCMDS(args[i+1], args, i);
        }
        else if (!strcmp(args[i], ">")){ //output redirect
            printf("Found >\n");
            redirectPos = i;
            return 2;
        }
        else if (!strcmp(args[i], "<")){ //input redirect
            printf("Found <\n");
            redirectPos = i;
            return 3;
        }
    }
    return 0;
}

void testPipe(char** args, int cmdPos){
    if (cmdPos > -1) {
        //Seperate front command and back command of pipes
        char **cmdFirst = malloc(sizeof(char *) * (512));
        char **cmdSecond = malloc(sizeof(char *) * (512));

        int cmd1len = 0, cmd2len = 0; //first and second command lengths
        for (int i = 0; i < cmdPos; i++){
            cmdFirst[i] = strdup(args[i]);
            cmd1len++;
        }
        for (int i = cmdPos + 1; i < lengthofArgs; i++)
        {
            cmdSecond[cmd2len] = strdup(args[i]);
            cmd2len++;
        }
        cmdFirst[cmd1len] = NULL; //set both to NULL
        cmdSecond[cmd2len] = NULL;

        // Create a pipe
        int fd[2];
        if (pipe(fd) == -1){
            printf("Piping Error");
            exit(EXIT_FAILURE);
        }
        else{
            pid_t pid1 = fork();
            if (pid1 == -1){
                printf("Forking Error");
                exit(EXIT_FAILURE);
            }
            else if (pid1 == 0){ //child process for first command
                close(fd[0]);
                dup2(fd[1], STDOUT_FILENO);
                close(fd[1]);
                lengthofArgs = cmd1len;
                checkCMDS("test", cmdFirst, 0);
                exit(EXIT_SUCCESS);
            }
            else{ //Creates child process for second command and returns to parent process
                pid_t pid2 = fork();
                if (pid2 == -1){
                    perror("fork");
                    exit(EXIT_FAILURE);
                }
                else if (pid2 == 0){ //child process for second command
                    close(fd[1]); // Close the write end of the pipe
                    dup2(fd[0], STDIN_FILENO);
                    close(fd[0]); // Close the read end of the pipe
                    // Execute the second command
                    lengthofArgs = cmd2len;
                    checkCMDS("test", cmdSecond, 0);
                    exit(EXIT_SUCCESS);
                }
                else{// Parent process: close both ends of the pipe and wait for children to finish
                    close(fd[0]);
                    close(fd[1]);
                    waitpid(pid1, NULL, 0);
                    waitpid(pid2, NULL, 0);
                    //Need to free all memory used to separate commands
                    for (int i = 0; cmdFirst[i] != NULL; i++){
                        free(cmdFirst[i]);
                    }
                    for (int i = 0; cmdSecond[i] != NULL; i++){
                        free(cmdSecond[i]);
                    }
                    free(cmdFirst);
                    free(cmdSecond);
                    return; //All process is done and completed
                }
            }
        }
    }
}

void testRedirect(char** args){

}
void intializeBatch(char** args, char** argv){ //activates batch mode
    int fd = open(argv[1], O_RDONLY);
    if (fd == -1) {
        printf("Error opening file: %s\n", argv[1]);
    }
    else {
        char bufferArr[2048], wordArr[200]; //bufferArr is for read() buffer, wordArr is for holding words, 46 because longest word is 45
        int bytesToRead, indexOfWord = 0;

        while ((bytesToRead = read(fd, bufferArr, sizeof(bufferArr) - 1)) > 0) {
            for (int i = 0; i < bytesToRead; i++) { //Loop through buffer to find individual words
                if (bufferArr[i] == '\n') { //Checks if its whitespace or end of line
                    if (!(indexOfWord < 1)) { //Checks if its at the end of an word
                        wordArr[indexOfWord] = '\0'; //Puts Null terminator for end of string
                        indexOfWord = 0;
                        args = splitLine(wordArr);
                        checkCMDS(wordArr, args, 0);
                        free(args);
                    }
                } 
                else {
                    wordArr[indexOfWord] = bufferArr[i];
                    indexOfWord++;
                }
            }
        }
        close(fd);
    }
}

void intializeShell(char** argv){ //intializes shell for either interactive or batch mode, main argument loop
    char* cmdLine;
	char** args;
	while(QUIT == 0){
        if (modeInteractive){
            printf("mysh> ");
		    cmdLine=readLine();
		    args=splitLine(cmdLine);
            checkCMDS(cmdLine, args, 0);
        }
        else{ //for batch mode
            intializeBatch(args, argv);
        }

        if (modeInteractive){
            free(cmdLine);
		    free(args);
        }
	}
}

void checkCMDS(char* cmdLine, char** args, int cmdPos){ //checks for what kind of CMD
    if (cmdLine[0] == '/'){
        printf("Path File\n");
    }
    else if (checkInternalCMDS(args, cmdPos)){}
    else if (checkExternalCMDS(args, 0)){}
}

int main(int argc, char** argv){ //main function to activate shell
    if (argc == 1){
        modeInteractive = 1;
        printf("Welcome to my Shell!\n");
        intializeShell(argv);
    }
    else if (argc == 2){
        modeInteractive = 0;
        intializeShell(argv);
    }
    else {
        printf("Invalid argument #\n");
    }

    return EXIT_SUCCESS;
}
