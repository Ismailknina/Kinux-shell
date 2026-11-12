#include "shell_config.h"

void display_help()
{
    printf("Available commands:\n");
    printf("\tcd <directory>      - Change the current directory.\n");
    printf("\tpwd                 - Print the current working directory.\n");
    printf("\techo <text>         - Print the given text.\n");
    printf("\tenv                 - Display all environment variables.\n");
    printf("\tsetenv VAR=value    - Set an environment variable.\n");
    printf("\tunsetenv <variable> - Remove an environment variable.\n");
    printf("\twhich <command>     - Locate an executable in the system's PATH.\n");
    printf("\t.help               - Display this help message.\n");
    printf("\texit or quit        - Exit the shell.\n");
}

// Check if the command is a built-in command and execute it accordingly.
int built_in_commands(char** args , char*** env , char* initial_directory)
{
    if (strcmp(args[0], "cd") == 0)
    {
        return command_cd(args , initial_directory);
    } 
    else if (strcmp(args[0], "pwd") == 0)
    {
        return command_pwd();
    } 
    else if (strcmp(args[0], "echo") == 0)
    {
        return command_echo(args , *env);
    } 
    else if (strcmp(args[0], "env") == 0)
    {
        return command_env(*env);
    } 
    else if (strcmp(args[0], "setenv") == 0)
    {
        *env = command_setenv(args , *env);
        return 0;
    } 
    else if (strcmp(args[0], ".help") == 0 || strcmp(args[0], "help") == 0)
    {
        display_help();
        return 0;
    } 
    else if (strcmp(args[0], "which") == 0)
    {
        return command_which(args , *env);
    } 
    else if (strcmp(args[0], "exit") == 0 || strcmp(args[0], "quit") == 0)
    {
        exit(EXIT_SUCCESS); // Exit the shell successfully.
    } 
    else
    {
        // Not a built-in command, execute it as an external command.
        return executors(args , *env);
    }

    return 0;
}

void run_shell(char** env){

    char* input = NULL;
    size_t input_size = 0;
    
    char** args;
    
    char* initial_directory = getcwd(NULL,0);

    printf("Type .help for a list of available commands.\n");

    while (1)
    {
        printf("[Kinux-Shell]>> "); // The user sees this and knows that the shell is ready to receive a command.

        if (getline(&input , &input_size , stdin) == -1) // EOF , Error , Ctrl+D
        {
            perror("getline");
            break;
        }
        //printf("Input %s" , input);

        args = split_commands(input);
        
        if (args && args[0])
        {
            built_in_commands(args , &env , initial_directory);
        }

        free_tokens(args); // Free the memory allocated for the command tokens.
    }

    free(input);
    free(initial_directory);
}


int main(int argc , char** argv , char** env){ 

    (void)argc; 
    (void)argv;

    run_shell(env); // Keep the shell running and waiting for commands.

    return 0;
}
