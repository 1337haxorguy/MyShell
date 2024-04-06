Angelo Dela Fuente add139
Christopher Chiaramonte cmc701

Testing Plan:

1. Testing the built in commands:
   Call the command
   ./mysh ./testing/test.txt
   to test all the required built ins

2. Testing the wildcards
   Call the command
   ./mysh ./testing/test2.txt
   to test the wildcards for where the token is before the wildcard,
   after the wildcard, and to test that the *.txt does not flag
   any files with a . in front of them

3. Testing Redirection
   Call the command
   ./mysh ./testing/test3.txt
   to test redirecting the output of a command to a file that does not exist
   to test setting the input of a file to a file itself

4. Testing Piping
   Call the command
   ./mysh ./testing/test4.txt
   to test piping two different commands together

5. Testing Conditionals
   Call the command
   ./mysh ./testing/test5.txt
   to test if an else statement works after a command fails
   To test if a then statement works after an else statement
   To test if an else statement works after a successful statement

The process:

1. Check the arguments to see if we should be in interactive mode or batch mode
2. Read lines line by line in from the input stream until there are none
3. Tokenize the lines into each word, and ensure <, >, and | are their own tokens
4. Check for any wildcard tokens, and if there are, use the glob function to expand them in the array
5. Command checks for else and then statements and also keeps track of whether or not the last command was successful or not
6. Execute the command, and check for pipes, each pipe has its own child process and recursively calls the execute function
7. Execute then checks for redirection with <, and >, which also uses dup2 to set the input or output, then this also calls another child process to execute the commands
8. At the end of the execute function, we check if the command is one of the built in commands, or if we should look within the system for the command, then we run it
