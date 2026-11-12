#include "shell_config.h"

int command_cd(char** args , char* initial_path)
{
    (void)initial_path; // Suppress unused parameter warning

    if(args[1] == NULL) printf("expected path after cd\n");

    else if(chdir(args[1])==0) printf("The directory has been changed to %s\n", args[1]);

    else perror("cd");

    return 0;
}

int command_pwd()
{
    char* cwd = NULL;

    cwd = getcwd(NULL,0);

    if(cwd != NULL) {printf("%s\n",cwd); free(cwd);}

    else perror("getcwd");

    return 0;
}

int command_echo(char** args , char** env)
{
    int new_line = 1;

    size_t i = 1;

    if(args[1] != NULL && strcmp(args[1], "-n")==0)
    {
        new_line = 0;
        i++;
    }

    for (; args[i] ; i++)
    {
        if(args[i][0] == '$')
        {
            char* env_var = NULL;
            size_t j = 0;

            // Search for the environment variable in the shell's environment array
            while(env[j] != NULL)
            {
                if(strncmp(env[j], args[i]+1, strlen(args[i]+1)) == 0 && env[j][strlen(args[i]+1)] == '=')
                {
                    env_var = env[j] + strlen(args[i]+1) + 1;
                    break;
                }
                j++;
            }

            if(env_var) printf("%s", env_var);

            else printf("Not Found ");
        }
        else printf("%s", args[i]);

        if(args[i+1] != NULL)
        {
            new_line = 0;
            printf(" ");
        }

        if(new_line) printf("\n");

        new_line = 1;

    }
    return 0;

}

// Function to print the environment variables
int command_env(char** env)
{
    size_t i = 0;

    while(env[i] != NULL)
    {
        printf("%s\n" , env[i]);
        i++;
    }
    return 0;
}

char* find_command_in_path(const char* command, char** env); // Forward declaration of the function to find a command in the PATH

int command_which(char** args , char** env)
{
    if (args[1] == NULL)
    {
        printf("Which: expected argument after which\n");
        return 1;   // to indicate that the operation failed or that the user supplied invalid input.
    }

    // List of built-in commands
    const char* built_in_commands[]={"cd","pwd","echo","env","setenv","unsetenv","which","exit","quit",NULL};

    // Searching through the built-in commands
    for (size_t i =0 ; built_in_commands[i] ; i++)
    {
        if (strcmp(args[1], built_in_commands[i]) == 0)
        {
            printf("%s is a built-in command\n", args[1]);
            return 0;
        }
    }

    // Searching for external commands
    char* full_path = find_command_in_path(args[1], env);
    if(full_path)
    {
        printf("%s\n", full_path);
        free(full_path); // free the allocated memory for full_path because find_command_in_path likely allocates memory for the path string.
        return 0;
    }
    else
    {
        printf("which: %s: command not found\n", args[1]);
        return 1; // to indicate that the operation failed or that the command was not found.
    }

}

// Function to search for the command in PATH
char* find_command_in_path(const char* command, char** env)
{
    char* path_env = NULL; // Variable to hold the PATH environment variable
    char* path_copy = NULL; // Variable to hold a copy of the PATH variable for tokenization
    char* token = NULL; // Variable to hold each directory in the PATH during tokenization
    char full_path[1024]; // Array to hold the full path of the command

    // Searching for the PATH environment variable in the provided env array
    for (size_t i = 0; env[i]; i++)
    {
        if (strncmp(env[i], "PATH=", 5) == 0) // Check if the current environment variable starts with "PATH="
        {
            path_env = env[i] + 5; // Skip the "PATH=" part to get the actual PATH value
            break; // Once we the found PATH, we don't need to continue searching.
        }
    }

    if (path_env == NULL) return NULL; // If PATH is not found, return NULL

    path_copy = strdup(path_env); // Create a copy of the PATH variable for tokenization
    if(path_copy == NULL)
    {
        perror("Failed to allocate memory for path_copy");
        return NULL; // Return NULL if memory allocation fails
    }

    token = strtok(path_copy, ":"); // Tokenize the PATH variable using ":" as the delimiter
    while (token != NULL)
    {
        size_t len = strlen(token); // Get the length of the current directory in PATH

        // Construct the full path to the command by appending the command name to the current directory
        if(token[len - 1] != '/') snprintf(full_path, sizeof(full_path), "%s/%s", token, command);
        else snprintf(full_path, sizeof(full_path), "%s%s", token, command);

        // Check if the constructed path is executable using access() function
        if (access(full_path, X_OK) == 0)
        {
            free(path_copy); // Free the allocated memory for path_copy before returning
            return strdup(full_path); // Return a copy of the full path to the command
        }

        token = strtok(NULL, ":"); // Get the next directory in PATH
    }

    free(path_copy); // Free the allocated memory for path_copy
    return NULL; // Return NULL if the command is not found in any directory in PATH

}

// Function to count the number of environment variables
int count_env_vars(char** env)
{
    size_t count = 0;
    while(env[count] != NULL) count++; // Increment the count for each non-NULL environment variable
    return (int)count;
}


char** command_setenv(char** args , char** env)
{
    if (args[1] == NULL )
    {
        printf("Usage: setenv VARIABLE=VALUE \t setenv <variable> <value> \n");
        return env; // Return the original environment if no arguments are provided
    }

    int env_count = count_env_vars(env); // Count the current number of environment variables

    char** new_env = malloc((env_count + 2) * sizeof(char*)); // Allocate memory for the new environment array (+1 for the new variable, +1 for NULL terminator)
    if (new_env == NULL) // Check if memory allocation was successful
    {
        perror("Failed to allocate memory for new environment");
        return env; // Return the original environment if memory allocation fails
    }

    for (int i = 0; i < env_count; i++)
    {
        new_env[i] = strdup(env[i]); // Copy existing environment variables to the new array

        if (!new_env[i])
        {
            perror("strdup");
            for (int j = 0; j < i; j++)
            {
                free(new_env[j]);
            }
            free(new_env);
            return env;
        }
    }

    char* new_var = NULL;
    if (args[2] == NULL)
    {
        new_var = strdup(args[1]);
    }
    else
    {
        new_var = malloc(strlen(args[1]) + strlen(args[2]) + 2);
        if (new_var)
        {
            sprintf(new_var , "%s=%s" , args[1] , args[2]);
        }
    }

    if(!new_var)
    {
        perror("malloc");
        for (int i = 0; i < env_count; i++)
        {
            free(new_env[i]);
        }
        free(new_env);
        return env;
    }

    new_env[env_count] = new_var;
    new_env[env_count + 1] = NULL;

    return new_env;

}

char** command_unsetenv(char** args, char** env)
{

    if (!args[1])
    {
        printf("Usage : unsetenv <variable> \n");
        return env;
    }

    int env_count = count_env_vars(env);

    char** new_env = malloc((env_count + 1) * sizeof(char*));

    if(!new_env)
    {
        perror("Failed to allocate memory for new environment");
        return env; // Return the original environment if memory allocation fails
    }

    int j=0 , found = 0;

    for (int i = 0 ; i < env_count ; i++)
    {
        if(strncmp(env[i],args[1],strlen(args[1])) == 0 &&
           env[i][strlen(args[1])] == '=')
        {
            found = 1;
        }
        else
        {
            new_env[j] = strdup(env[i]);

            if(!new_env[j])
            {
                perror("strdup");

                for (int k = 0; k < j; k++)
                {
                    free(new_env[k]);
                }

                free(new_env);
                return env;
            }

            j++;
        }
    }

    if(!found)
    {
        printf("Variable %s not found in environment \n" , args[1]);

        for (int i = 0; i < j; i++)
        {
            free(new_env[i]);
        }

        free(new_env);
        return env;
    }

    new_env[j] = NULL;

    return new_env;
}