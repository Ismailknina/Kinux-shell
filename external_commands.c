#include "shell_config.h"

int executors(char** args , char** env)
{
    pid_t pid ;
    int status ;

    // Check if there is a valid command to execute
    if (!args || !args[0]) return 0;

    pid = fork();

    // Handling fork failure
    if(pid == -1)
    {
        perror("Fork Problem");
        return 1;
    }

    if(pid == 0)
    {
        if(child_process(args,env))
        {
            _exit(EXIT_FAILURE); // Terminate the child process if execution fails.
        }

        _exit(EXIT_SUCCESS);
    }
    else // Parent Process
    {
        if(waitpid(pid,&status,0) == -1)  // Checking if there is an Error
        {
            perror("waitpid");
            return 1;
        }

        if (WIFSIGNALED(status)) // Was it terminated by a signal?
        {
            printf("Process terminated by signal: %d\n", WTERMSIG(status));// Which signal terminated it?
            return 128 + WTERMSIG(status);
        }

        if (WIFEXITED(status))
        {
            return WEXITSTATUS(status); // Return the exit status of the external command.
        }
    }

    return 1;
}

int child_process(char** args , char** env)
{
    char* path_string = NULL;
    int num_paths = 0; // Number of directories extracted from PATH
    char** path_list = NULL;

    // Check if the command already contains a path
    if (strchr(args[0], '/') != NULL)
    {
        execve(args[0], args, env);
        perror("execve");
        return 1;
    }

    path_string = get_path(env); // Finding the env string beginning with "PATH="

    if (path_string != NULL)
    {
        path_list = split_paths(path_string,&num_paths);
    }

    // Loop through every PATH directory
    for (int i=0; i < num_paths; i++)
    {
        // Construct the full executable path
        char full_path[MAX_INPUT_SIZE]; // Limit of characters
        int path_length = snprintf(full_path, sizeof(full_path), "%s/%s", path_list[i], args[0]);

        if (path_length < 0 || path_length >= (int)sizeof(full_path))
        {
            continue; // Skip paths that are too long.
        }

        // Check if the file is executable
        if(access(full_path,X_OK)== 0)
        {
            // This tells the operating system to replace the child process's current program with full_path
            execve(full_path,args,env);
        }
    }

    // Freeing Memory
    for(int i = 0 ; i < num_paths ; i++)
    {
        free(path_list[i]);
    }

    free(path_list);
    free(path_string);

    // Try executing the command in the current working directory
    char* cwd = NULL;
    cwd = getcwd(NULL, 0);

    if(cwd == NULL)
    {
        perror("getcwd");
        return 1;
    }

    // Construct the full path using the current working directory
    char full_cwd_path[MAX_INPUT_SIZE];
    int path_length = snprintf(full_cwd_path, sizeof(full_cwd_path), "%s/%s", cwd, args[0]);

    if (path_length < 0 || path_length >= (int)sizeof(full_cwd_path))
    {
        fprintf(stderr, "Command path is too long\n");
        free(cwd);
        return 1;
    }

    // Try to execute the command
    execve(full_cwd_path, args, env);

    perror("execve");
    free(cwd);

    return 1;
}

// Fetches the PATH environment variable
char* get_path(char** env)
{
    if (!env) return NULL;

    for (int i = 0; env[i]; i++)
    {
        if(strncmp(env[i], "PATH=", 5) == 0)
        {
            char* path_string = strdup(env[i] + 5);

            if (!path_string)
            {
                perror("strdup");
            }

            return path_string;
        }
    }

    return NULL;
}

// The purpose is to transform a PATH string into an array of directory strings.
char** split_paths(char* paths, int* count)
{
    char** result = NULL;
    char** temp = NULL;
    char* token;
    char* paths_copy;

    *count = 0;

    if (!paths) return NULL;

    paths_copy = strdup(paths);

    if (!paths_copy)
    {
        perror("strdup");
        return NULL;
    }

    token = strtok(paths_copy, ":");

    while (token)
    {
        // Allocate space for the new path and the final NULL pointer
        temp = realloc(result, ((*count + 2) * sizeof(char*)));

        if (!temp)
        {
            perror("realloc");

            for (int i = 0; i < *count; i++)
            {
                free(result[i]);
            }

            free(result);
            free(paths_copy);
            *count = 0;
            return NULL;
        }

        result = temp;

        // Copy the current directory path
        result[*count] = strdup(token);

        if (!result[*count])
        {
            perror("strdup");

            for (int i = 0; i < *count; i++)
            {
                free(result[i]);
            }

            free(result);
            free(paths_copy);
            *count = 0;
            return NULL;
        }

        (*count)++;

        result[*count] = NULL; // Keep the array NULL-terminated

        token = strtok(NULL, ":");
    }

    free(paths_copy);

    return result;
}