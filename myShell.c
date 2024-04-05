#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <fnmatch.h>
#include <glob.h>

#define MAX_COMMAND_LENGTH 1024
#define MAX_TOKEN_LENGTH 100
#define MAX_TOKENS 100

void cd(const char *directory);
char *readline(int input_stream);
char **tokenize(char *line);
void command(char **arguments);
void exit_command(char **arguments);
void pwd(char **arguments);
void which(char *arguments);
void execute(char **arguments);
char **globTokens(char **tokens);

int main(int argc, char *argv[])
{
    system("clear");
    int interactive_mode = isatty(STDIN_FILENO);
    int input_stream;

    // Determine input stream
    if (argc == 2)
    {
        input_stream = open(argv[1], O_RDONLY);
        if (input_stream == -1)
        {
            perror("Error opening file");
            return 1;
        }
        interactive_mode = 0; // Batch mode
    }
    else
    {
        input_stream = STDIN_FILENO;
    }

    if (interactive_mode)
    {
        printf("Welcome to my shell!\n");
        fflush(stdout); // Ensure the prompt is printed immediately
    }

    while (1)
    {
        if (interactive_mode)
        {
            printf("mysh> ");
            fflush(stdout); // Ensure the prompt is printed immediately
        }
        // Read a line from the input stream
        char *line = readline(input_stream);
        if (line == NULL)
        {
            // Error or end of file occurred
            break;
        }

        char **tokens = tokenize(line);
        char **expandedTokens = globTokens(tokens);

        command(expandedTokens);

        free(tokens);
        free(line);
        free(expandedTokens);
    }

    if (argc == 2)
    {
        close(input_stream);
    }

    return 0;
}

void cd(const char *directory) {
    if (directory == NULL) {
        fprintf(stderr, "cd: missing argument\n");
        return;
    }

    if (chdir(directory) == -1) {
        perror("cd");
    }
}

void exit_command(char **arguments) // Renamed exit to exit_command
{
    for (int i = 1; arguments[i] != NULL; i++)
    {
        printf("%s\n", arguments[i]);
        fflush(stdout); // Ensure the prompt is printed immediately
    }
    printf("Exiting my shell\n");
    fflush(stdout); // Ensure the prompt is printed immediately

    // Terminate the shell
    exit(0);
}

void pwd(char **arguments) {
    char cwd[MAX_COMMAND_LENGTH]; // Buffer to store current working directory
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("%s\n", cwd); // Print current working directory
    }
    else {
        perror("getcwd() error");
    }
}

void which(char *program_name) {
    // Check if the provided program name is a built-in
    if (strcmp(program_name, "cd") == 0 || strcmp(program_name, "exit") == 0 || strcmp(program_name, "pwd") == 0) {
        printf("Error: '%s' is a built-in command\n", program_name);
        return;
    }

    // Get the PATH environment variable
    char *path_env = getenv("PATH");
    if (path_env == NULL)
    {
        printf("Error: PATH environment variable is not set\n");
        return;
    }

    // Tokenize PATH to get individual directories
    char *path_env_copy = strdup(path_env); // Make a copy of path_env
    char *path = strtok(path_env_copy, ":");
    while (path != NULL)
    {
        // Construct the full path to the program
        char full_path[MAX_COMMAND_LENGTH];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, program_name);

        // Check if the program exists and is executable
        if (access(full_path, X_OK) == 0)
        {
            printf("%s\n", full_path);
            free(path_env_copy); // Free the copy of path_env
            return;
        }

        // Move to the next directory in PATH
        path = strtok(NULL, ":");
    }

    // Program not found in any directory in PATH
    printf("Error: '%s' not found in PATH\n", program_name);
    free(path_env_copy); // Free the copy of path_env
}

char *readline(int input_stream)
{
    char *input = malloc(sizeof(char) * MAX_COMMAND_LENGTH);

    int index = 0;
    char c;

    while (1)
    {
        ssize_t bytes_read = read(input_stream, &c, 1);

        if (bytes_read == -1)
        {
            perror("read");
            free(input); // Free allocated memory before returning NULL
            return NULL;
        }
        else if (bytes_read == 0)
        {
            // End of file reached
            if (index == 0)
            {
                // No input was read, return NULL to indicate end of input
                free(input);
                return NULL;
            }
            break;
        }

        if (c == '\n')
        {
            break; // End loop when newline is encountered
        }

        if (index >= MAX_COMMAND_LENGTH - 1)
        {
            printf("User input too long. Please input args less than 1024 characters.\n");
            break;
        }

        input[index++] = c; // Store character in input buffer
    }

    input[index] = '\0'; // Null-terminate the string
    return input;
}

char **tokenize(char *line)
{
    int i = 0;
    char **arguments = malloc(sizeof(char *) * MAX_TOKENS);
    const char *delimiters = " "; // Include <, >, |, and whitespace characters
    char *token = strtok(line, delimiters);

    while (token != NULL)
    {

        char *special_char = strpbrk(token, "<>|");
        if (special_char != NULL)
        {
            char special_char_value = *special_char;

            // If the special character is at the beginning of the token, add it separately
            if (special_char == token)
            {
                arguments[i] = malloc(2); // One character and null terminator
                arguments[i][0] = special_char_value;
                arguments[i][1] = '\0';
                i++;

                token = special_char + 1;
            }
            else
            {
                *special_char = '\0'; // Replace the special character with null terminator
                arguments[i] = strdup(token);
                i++;

                // Add the special character token
                arguments[i] = malloc(2);             // One character and null terminator
                arguments[i][0] = special_char_value; // Store the special character value
                arguments[i][1] = '\0';               // Null-terminate the string
                i++;

                // Move to the next part of the string after the special character
                token = special_char + 1;
            }
        }
        else
        {
            if (*token != '\0')
            {
                arguments[i] = strdup(token);
                i++;
            }
            token = strtok(NULL, delimiters);
        }
    }
    arguments[i] = NULL;

    return arguments;
}

void command(char **arguments) {
    static int previous_exit_status = 0; // Initialize previous exit status

    if (arguments == NULL || arguments[0] == NULL) {
        return; // No command to execute
    }

    // Check if the command is a conditional (then or else)
    if (strcmp(arguments[0], "then") == 0 || strcmp(arguments[0], "else") == 0) {
        printf("we got else, previous exit status: %d\n", previous_exit_status);
        if ((strcmp(arguments[0], "then") == 0 && previous_exit_status != 1) ||
            (strcmp(arguments[0], "else") == 0 && previous_exit_status == 1)) {
            int i = 1;
            while (arguments[i] != NULL) {
                command(arguments + i);
                i++;
            }
        }
        return;
    }

    if (strcmp(arguments[0], "cd") == 0) {
        if (arguments[1] != NULL) {
            cd(arguments[1]);
        }
    } else if (strcmp(arguments[0], "exit") == 0) {
        exit_command(arguments);
    } else if (strcmp(arguments[0], "pwd") == 0) {
        pwd(arguments);
    } else if (strcmp(arguments[0], "which") == 0) {
        which(arguments[1]);
    } else {
        execute(arguments);
    }

    // Set previous_exit_status based on the exit status of the executed command
    int status;
    wait(&status);
    if (WIFEXITED(status)) {
        previous_exit_status = WEXITSTATUS(status);
    } else {
        previous_exit_status = 1; // Command execution failed
    }
}

void execute(char **arguments) {
    pid_t pid = fork();
    if (pid < 0){
        perror("fork");
        exit(EXIT_FAILURE);
    }
    else if (pid == 0){
        int input_fd = STDIN_FILENO;
        int output_fd = STDOUT_FILENO;
        for (int i = 0; arguments[i] != NULL; i++){
            if (strcmp(arguments[i], "<") == 0){
                // Input redirection
                input_fd = open(arguments[i + 1], O_RDONLY);
                if (input_fd == -1)
                {
                    perror("open");
                    exit(EXIT_FAILURE);
                }
                // Remove the "<" and filename tokens
                arguments[i] = NULL;
                arguments[i + 1] = NULL;
            }
            else if (strcmp(arguments[i], ">") == 0){
                // Output redirection
                output_fd = open(arguments[i + 1], O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP);
                if (output_fd == -1)
                {
                    perror("open");
                    exit(EXIT_FAILURE);
                }
                // Remove the ">" and filename tokens
                arguments[i] = NULL;
                arguments[i + 1] = NULL;
            } else if (strcmp(arguments[i], "|") == 0) {
                
            }
        }
        // Redirect file descriptors
        if (dup2(input_fd, STDIN_FILENO) == -1){
            perror("dup2");
            exit(EXIT_FAILURE);
        }
        if (dup2(output_fd, STDOUT_FILENO) == -1){
            perror("dup2");
            exit(EXIT_FAILURE);
        }
        // Close file descriptors if they were changed
        if (input_fd != STDIN_FILENO){
            close(input_fd);
        }
        if (output_fd != STDOUT_FILENO){
            close(output_fd);
        }
        // Execute the command
        if (arguments[0][0] == '/'){
            // Pathname provided, execute directly
            execv(arguments[0], arguments);
        }
        else{
            // Bare name provided, search in specified directories
            const char *directories[] = {"/usr/local/bin", "/usr/bin", "/bin", NULL};
            for (int i = 0; directories[i] != NULL; ++i){
                char full_path[MAX_COMMAND_LENGTH];
                snprintf(full_path, sizeof(full_path), "%s/%s", directories[i], arguments[0]);
                if (access(full_path, X_OK) == 0){
                    // Execute the executable
                    execv(full_path, arguments);
                }
            }
            fprintf(stderr, "Error: '%s' not found\n", arguments[0]);
            exit(EXIT_FAILURE);
        }
    }
    else {
        int status;
        waitpid(pid, &status, 0);
    }
}

char **globTokens(char **tokens)
{
    char **expandedTokens = malloc(sizeof(char *) * (MAX_TOKENS + 1)); // +1 for NULL terminator
    int index = 0;

    // Iterate through each token
    for (int i = 0; tokens[i] != NULL; i++)
    {
        char *token = tokens[i];
        glob_t glob_result;

        char *asterisk_position = strchr(token, '*');
        if (asterisk_position != NULL)
        {
            int glob_status = glob(token, 0, NULL, &glob_result);

            if (glob_status == 0)
            {
                for (size_t j = 0; j < glob_result.gl_pathc; j++)
                {
                    // Allocate memory for the matched filename
                    expandedTokens[index] = strdup(glob_result.gl_pathv[j]);
                    index++;
                }
                globfree(&glob_result);
            }
            else if (glob_status == GLOB_NOMATCH)
            {
                printf("zsh: no matches found: %s\n", token);
                return NULL;
            }
            else
            {
                printf("Error occurred during globbing.\n");
            }
        }
        else
        {
            // If no asterisk, add the token unchanged
            expandedTokens[index] = strdup(token);
            index++;
        }
    }

    // Add NULL terminator to expandedTokens
    expandedTokens[index] = NULL;

    return expandedTokens;
}
