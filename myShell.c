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
void exitCommand(char **arguments);
void pwd(char **arguments);
void which(char *arguments);
void execute(char **arguments);
char **globTokens(char **tokens);

int previous_exit_status = 1;
int interactive_mode = 0;

int main(int argc, char *argv[]) {
    int interactive_mode = isatty(STDIN_FILENO);
    int input_stream;

    if (!interactive_mode) {
        system("clear");
    }

    // Determine input stream
    if (argc == 2) {
        input_stream = open(argv[1], O_RDONLY);
        if (input_stream == -1) {
            perror("Error opening file");
            return 1;
        }
        interactive_mode = 0; // Batch mode
    }
    else {
        input_stream = STDIN_FILENO;
    }

    if (interactive_mode) {
        printf("Welcome to my shell!\n");
        fflush(stdout); // Ensure the prompt is printed immediately
    }

    while (1) {
        if (interactive_mode) {
            printf("mysh> ");
            fflush(stdout); // Ensure the prompt is printed immediately
        }
        // Read a line from the input stream
        char *line = readline(input_stream);
        if (line == NULL) {
            break;
        }

        char **tokens = tokenize(line);
        char **expandedTokens = globTokens(tokens);

        command(expandedTokens);

        free(tokens);
        free(line);
        free(expandedTokens);
    }

    if (argc == 2) {
        close(input_stream);
    }

    return 0;
}

void cd(const char *directory) {
    if (directory == NULL) {
        previous_exit_status = 1;
        fprintf(stderr, "cd: missing argument\n");
        return;
    }

    if (chdir(directory) == -1) {
        previous_exit_status = 1;
        perror("cd");
    }
}

void exitCommand(char **arguments) {
    for (int i = 1; arguments[i] != NULL; i++) {
        printf("%s\n", arguments[i]);
        fflush(stdout); 
    }

    if (!interactive_mode) {
        printf("Exiting my shell\n");
        fflush(stdout);
    }

    exit(0);
}

void pwd(char **arguments) {
    char cwd[MAX_COMMAND_LENGTH]; // Buffer to store current working directory
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("%s\n", cwd); // Print current working directory
    }
    else {
        previous_exit_status = 1;
        perror("getcwd() error");
    }
}

void which(char *program_name) {
    if (strcmp(program_name, "cd") == 0 || strcmp(program_name, "exit") == 0 || strcmp(program_name, "pwd") == 0) {
        printf("Error: '%s' is a built-in command\n", program_name);
        previous_exit_status = 1;
        return;
    }

    char *path_env = getenv("PATH");
    if (path_env == NULL) {
        printf("Error: PATH environment variable is not set\n");
        return;
    }
    char *path_env_copy = strdup(path_env);
    char *path = strtok(path_env_copy, ":");
    while (path != NULL) {
        char full_path[MAX_COMMAND_LENGTH];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, program_name);

        if (access(full_path, X_OK) == 0) {
            printf("%s\n", full_path);
            free(path_env_copy); 
            return;
        }
        path = strtok(NULL, ":");
    }

    previous_exit_status = 1;
    printf("Error: '%s' not found in PATH\n", program_name);
    free(path_env_copy); 
} 

char *readline(int input_stream) {
    char *input = malloc(sizeof(char) * MAX_COMMAND_LENGTH);

    int index = 0;
    char c;

    while (1) {
        ssize_t bytes_read = read(input_stream, &c, 1);

        if (bytes_read == -1) {
            perror("read");
            free(input); // Free allocated memory before returning NULL
            return NULL;
        }
        else if (bytes_read == 0) {
            // End of file reached
            if (index == 0) {
                free(input);
                return NULL;
            }
            break;
        }

        if (c == '\n') {
            break; // End loop when newline is encountered
        }

        if (index >= MAX_COMMAND_LENGTH - 1) {
            printf("User input too long. Please input args less than 1024 characters.\n");
            break;
        }

        input[index++] = c; // Store character in input buffer
    }

    input[index] = '\0'; // Null-terminate the string
    return input;
}

char **tokenize(char *line) {
    int i = 0;
    char **arguments = malloc(sizeof(char *) * MAX_TOKENS);
    const char *delimiters = " ";
    char *token = strtok(line, delimiters);

    while (token != NULL) {
        char *special_char = strpbrk(token, "<>|");
        if (special_char != NULL) {
            char special_char_value = *special_char;

            //if special character is at the beginning of the token, add it separately
            if (special_char == token) {
                arguments[i] = malloc(2); // one character and \n
                arguments[i][0] = special_char_value;
                arguments[i][1] = '\0';
                i++;

                token = special_char + 1;
            }
            else {
                *special_char = '\0'; // Replace the special character with null terminator
                arguments[i] = strdup(token);
                i++;

                // Add the special character token
                arguments[i] = malloc(2);             
                arguments[i][0] = special_char_value; 
                arguments[i][1] = '\0';               
                i++;

                // Move to the next part of the string after the special character
                token = special_char + 1;
            }
        }
        else {
            if (*token != '\0') {
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
    if (arguments == NULL || arguments[0] == NULL) {
        return;
    }

    if (strcmp(arguments[0], "then") == 0 || strcmp(arguments[0], "else") == 0) {
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
    execute(arguments);
}

void execute(char **arguments)
{
    int pipe_index = -1;
    int input_redirection = 0;
    int output_redirection = 0;
    char *input_file = NULL;
    char *output_file = NULL;

    // Find the index of the pipe character '|' and redirection symbols '<' and '>'
    for (int i = 0; arguments[i] != NULL; i++) {
        if (strcmp(arguments[i], "|") == 0) {
            pipe_index = i;
        }
    }
    
    if (pipe_index != -1) {
        // Split the command into two parts: before and after the pipe
        char **command1 = malloc(sizeof(char *) * (pipe_index + 1));
        char **command2 = malloc(sizeof(char *) * (MAX_TOKENS - pipe_index));

        // Copy the first part of the command
        for (int i = 0; i < pipe_index; i++)
        {
            command1[i] = strdup(arguments[i]);
        }
        command1[pipe_index] = NULL;

        // Copy the second part of the command
        int j = 0;
        for (int i = pipe_index + 1; arguments[i] != NULL; i++)
        {
            command2[j++] = strdup(arguments[i]);
        }
        command2[j] = NULL;

        // Create a pipe
        int fd[2];
        if (pipe(fd) == -1)
        {
            perror("pipe");
            exit(EXIT_FAILURE);
        }

        // Fork a child process to execute the first command
        pid_t pid1 = fork();
        if (pid1 == -1)
        {
            perror("fork");
            exit(EXIT_FAILURE);
        }
        else if (pid1 == 0)
        {
            // Child process: redirect stdout to the write end of the pipe
            close(fd[0]); // Close the read end of the pipe
            dup2(fd[1], STDOUT_FILENO);
            close(fd[1]); // Close the write end of the pipe
            // Execute the first command
            execute(command1);
            exit(EXIT_SUCCESS);
        }
        else
        {
            // Parent process: fork another child process to execute the second command
            pid_t pid2 = fork();
            if (pid2 == -1)
            {
                perror("fork");
                exit(EXIT_FAILURE);
            }
            else if (pid2 == 0)
            {
                // Child process: redirect stdin to the read end of the pipe
                close(fd[1]); // Close the write end of the pipe
                dup2(fd[0], STDIN_FILENO);
                close(fd[0]); // Close the read end of the pipe
                // Execute the second command
                execute(command2);
                exit(EXIT_SUCCESS);
            }
            else
            {
                // Parent process: close both ends of the pipe and wait for children to finish
                close(fd[0]);
                close(fd[1]);
                waitpid(pid1, NULL, 0);
                waitpid(pid2, NULL, 0);
                // Free memory allocated for command parts
                for (int i = 0; command1[i] != NULL; i++)
                {
                    free(command1[i]);
                }
                for (int i = 0; command2[i] != NULL; i++)
                {
                    free(command2[i]);
                }
                free(command1);
                free(command2);
                return;
            }
        }
    }

    for (int i = 0; arguments[i] != NULL; i++) {
        if (strcmp(arguments[i], "<") == 0) {
            input_redirection = i;
            input_file = arguments[i + 1]; // Next token is the input file
            // Remove '<' and the filename from arguments
            for (int j = i; arguments[j] != NULL; j++)
            {
                arguments[j] = arguments[j + 2];
            }
            i -= 2; // Adjust i to account for the removed tokens
        }
        else if (strcmp(arguments[i], ">") == 0)
        {
            output_redirection = i;
            output_file = arguments[i + 1]; // Next token is the output file
            // Remove '>' and the filename from arguments
            for (int j = i; arguments[j] != NULL; j++)
            {
                arguments[j] = arguments[j + 2];
            }
            i -= 2; // Adjust i to account for the removed tokens
        }

    }

        if (input_redirection) {
            pid_t input_pid = fork();
            if (input_pid == -1) {
                perror("fork");
                exit(EXIT_FAILURE);
            } else if (input_pid == 0) {
                int fd_in = open(input_file, O_RDONLY);
                if (fd_in == -1) {
                    perror("open");
                    exit(EXIT_FAILURE);
                }
                if (dup2(fd_in, STDIN_FILENO) == -1) {
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }
                close(fd_in);  // Close the file descriptor
                execute(arguments);  // Execute recursively
                exit(EXIT_SUCCESS);
            }
        }

        // Fork a child process for output redirection
        if (output_redirection) {
            pid_t output_pid = fork();
            if (output_pid == -1) {
                perror("fork");
                exit(EXIT_FAILURE);
            } else if (output_pid == 0) {
                int fd_out = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd_out == -1) {
                    perror("open");
                    exit(EXIT_FAILURE);
                }
                if (dup2(fd_out, STDOUT_FILENO) == -1) {
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }
                close(fd_out);  // Close the file descriptor
                execute(arguments);  // Execute recursively
                exit(EXIT_SUCCESS);
            }
        }

        // Wait for child processes to finish
        if (input_redirection) {
            wait(NULL);
        }
        if (output_redirection) {
            wait(NULL);
        }
if (!output_redirection && !input_redirection) {
    // If no pipe found, execute the command as usual
    if (strcmp(arguments[0], "cd") == 0)
    {
        cd(arguments[1]);
    }
    else if (strcmp(arguments[0], "exit") == 0)
    {
        exitCommand(arguments);
    }
    else if (strcmp(arguments[0], "pwd") == 0)
    {
        pwd(arguments);
    }
    else if (strcmp(arguments[0], "which") == 0)
    {
        which(arguments[1]);
    }
    else if (arguments[0][0] == '/')
    {
        // Fork a child process
        pid_t pid = fork();
        if (pid == 0)
        {
            // Child process: execute the command
            execv(arguments[0], arguments);
            // If execv fails, print an error message and exit child process
            perror("execv");
        }
        else
        {
            // Parent process: wait for the child process to finish
            int status;
            waitpid(pid, &status, 0);
            // Update previous_exit_status based on the exit status of the child process
            if (WIFEXITED(status))
            {
                previous_exit_status = WEXITSTATUS(status);
            }
        }
        return;
    }
    else
    {
        // Search for the command in specified directories and execute it
        const char *directories[] = {"/usr/local/bin", "/usr/bin", "/bin", NULL};
        for (int i = 0; i < 3; ++i)
        {
            char full_path[MAX_COMMAND_LENGTH];
            snprintf(full_path, sizeof(full_path), "%s/%s", directories[i], arguments[0]);
            if (access(full_path, X_OK) == 0)
            {
                // Fork a child process
                pid_t pid = fork();
                if (pid == 0)
                {
                    // Child process: execute the command
                    execv(full_path, arguments);
                    // If execv fails, print an error message and exit child process
                    perror("execv");
                }
                else
                {
                    // Parent process: wait for the child process to finish
                    int status;
                    waitpid(pid, &status, 0);
                    // Update previous_exit_status based on the exit status of the child process
                    if (WIFEXITED(status))
                    {
                        previous_exit_status = WEXITSTATUS(status);
                    }
                    return;
                }
            }
        }
        // If the command is not found in any directory, print an error message and update previous_exit_status
        fprintf(stderr, "Error: '%s' not found\n", arguments[0]);
        previous_exit_status = 1;
    }
}
    return;
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
