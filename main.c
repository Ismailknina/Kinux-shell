#include "shell_config.h"

int built_in_commands(char** args , char** env , char* initial_directory)
{
    // To avoid unused parameter warning (for the version where You see this the commands using env are commented out).
    (void) env; 
    // To avoid unused parameter warning (for the version where You see this the commands using initial_directory are commented out).
    (void) initial_directory;

    // Check if the command is a built-in command and execute it accordingly.
    if (strcmp(args[0], "cd") == 0)
    {
        return command_cd(args , initial_directory);
    } 
    else if (strcmp(args[0], "help") == 0)
    {
        // return command_help();
    } 
    else if (strcmp(args[0], "env") == 0)
    {
        return command_env(env);
    } 
    else if (strcmp(args[0], "setenv") == 0)
    {
        return command_setenv(args , env);
    } 
    else if (strcmp(args[0], "unsetenv") == 0)
    {
        // return command_unsetenv(args , env);
    } 
    else if (strcmp(args[0], "which") == 0)
    {
        return command_which(args , env);
    } 
    else if (strcmp(args[0], "exit") == 0 || strcmp(args[0], "quit") == 0)
    {
        exit(EXIT_SUCCESS); // Exit the shell successfully.
    } 
    else
    {
        // Not a built-in command.
        return 1; 
    }
    
    return 0;

}

void run_shell(char** env){

    char* input = NULL;
    size_t input_size = 0;
    
    char** args;
    
    char* initial_directory = getcwd(NULL,0);

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
        
        if (args[0])
        {
            built_in_commands(args , env , initial_directory);
        }

    }
    free_tokens(args); // Free the memory allocated for the command tokens.


}


int main(int argc , char** argv , char** env){ 

    (void)argc; 
    (void)argv;

    run_shell(env); // Keep the shell running and waiting for commands.

    return 0;
}