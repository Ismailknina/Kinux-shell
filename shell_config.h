#ifndef SHELL_CONFIG_H
#define SHELL_CONFIG_H

// Provides [printf],[scanf],[getline],[perror],[stdin] 
# include <stdio.h> 
// Provides [malloc],[free],[exit],[EXIT_SUCCESS],[EXIT_FAILURE]
# include <stdlib.h> 
// Provides [strcmp],[strdup],[strlen]
# include <string.h> 
// Provides [getcwd]
# include <unistd.h> 
// Provides [size_t (It is also available through stdlib.h and stdio.h)], [NULL]
#include <stddef.h>  
// 
#include <sys/types.h>
// Provides [waitpid]
#include <sys/wait.h> // Is working on Ubuntu and it's not in my MSYS2 UCRT64 enivironment what I am using in my machine 

// It tells the preprocessor to replace every occurrence of MAX_INPUT_SIZE with 1024 before compilation. It isn't a variable and doesn't allocate memory.
# define MAX_INPUT_SIZE 1024 

// Function to split the input string into commands based on spaces , Newlines , Carriage returns , Tabs , Bell characters.
char** split_commands(char* input);
// Function to free the memory allocated for the array of command strings.
void free_tokens(char** tokens);

// Built-in function implementations
int command_cd(char** args, char* initial_directory);
int command_pwd(void);
int command_echo(char** args, char** env);
int command_env(char** env);
int command_which(char** args, char** env);
char** command_setenv(char** args, char** env);
char** command_unsetenv(char** args, char** env);

// Executor
int executors(char** args, char** env);
int child_process(char** args, char** env);

// Path functions
char* get_path(char** env);
char** split_paths(char* paths, int* count);

// Function to know if a command is built-in like "cd", "env", "setenv", "unsetenv", "which", and "exit".
int built_in_commands(char** args , char*** env , char* initial_directory);

#endif
