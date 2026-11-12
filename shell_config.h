#ifndef SHELL_CONFIG_H
#define SHELL_CONFIG_H

// Provides [Printf],[scanf],[getline],[perror],[stdin] 
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
// 
#if !defined(_WIN32) && !defined(_WIN64)
#include <sys/wait.h>
#endif

// It tells the preprocessor to replace every occurrence of MAX_INPUT with 1024 before compilation. It isn't a variable and doesn't allocate memory.
# define MAX_INPUT_SIZE 1024 

// Function to split the input string into commands based on spaces , Newlines , Carriage returns , Tabs , Bell characters.
char** split_commands(char* input);
// Function to free the memory allocated for the array of command strings.
void free_tokens(char** tokens);
// Function to know if a command is built-in like "cd", "env", "setenv", "unsetenv", "which", and "exit".
int built_in_commands(char** args , char** env , char* initial_directory);

#endif 