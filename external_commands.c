# include <shell_config.h>

int executors(char** args , char** env)
{
    pid_t pid ;
    int status ;

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
            perror("execve");
            return 1;
        }
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
        }
        
    }
    return 1;

}

int child_process(char** args , char** env)
{
    char* path_string = get_path(env); // Finding the env string beginning with "PATH="
    int num_paths; // Number of directories extracted from PATH
    char** path_list = split_paths(path_string,&num_paths); // will update this variable through its second parameter.

    // Loop through every PATH directory
    for (int i=0; i < num_paths; i++)
    {
        // Construct the full executable path
        char full_path[MAX_INPUT_SIZE]; // Limit of charachters
        snprintf(full_path, sizeof(full_path), "%s/%s", path_list[i], args[0]);

        // check is the file is executable
        if(access(full_path,X_OK)== 0)
        {
            // This tells the operating system to replace the child process's current program with full_path
            execve(full_path,args,env); 
        }
    }

    // Freeing Memory
    for(int i = 0 ; path_list[i] ; i++) free(path_list[i]);
    free(path_string) ; 
    free(path_list);
    
    // Try executing the command in the current working directory
    char* cwd = NULL;
    cwd = getcwd(NULL, 0);

    // 
    if(cwd == NULL) { perror("getcwd"); return 1; }

    // 
    char full_cwd_path[MAX_INPUT_SIZE];
    snprintf(full_cwd_path, sizeof(full_cwd_path), "%s/%s", cwd, args[0]);
    execve(full_cwd_path, args, env);

    // 
    perror("execve");
    

    return 1;

}


// 
char* get_path(char** env) 
{
    for (int i = 0; env[i]; i++) 
    {
        if(strncmp(env[i], "PATH=", 5) == 0) return my_strdup(env[i] + 5);
    }
    return NULL;
}

// The purpose is to transform a PATH string into an array of directory strings.
char** split_paths(char* paths, int* count)
{
    char** result = NULL;
    char* token;
    size_t size_of_path = my_strlen(paths);
    char paths_copy[size_of_path];

    // 
    strncpy(paths_copy, paths, sizeof(paths_copy));
    paths_copy[sizeof(paths_copy) - 1] = '\0';

    // 
    token = my_strtok(paths_copy, ":");
    *count = 0;

    // 
    while (token) 
    {
        // 
        result = realloc(result, ((*count + 1) * sizeof(char*)));
        if (!result) 
        {
            perror("realloc");
            return NULL;
        }

        //
        result[*count] = my_strdup(token);
        if (!result[*count]) 
        {
            perror("my_strdup");
            return NULL;
        }
        
        // 
        (*count)++;
        token = my_strtok(NULL, ":");
    }

    return result;
}