#include "shell_config.h"

char** split_commands(char* input){
    size_t buffer_max = MAX_INPUT_SIZE;
    char** tokens = malloc(buffer_max * sizeof(char*));
    if (!tokens){
        perror("Memory allocation failed");
        exit(EXIT_FAILURE);
    }

    tokens[0] = NULL; // Initialize the token array safeely

    char* token = NULL;
    size_t position = 0; // index of the current token in the tokens array
    size_t token_length = 0;

    for(size_t i =0; input[i];i++)
    {

        while (input[i]== ' ' || input[i] =='\n' || input[i]=='\r' || input[i] == '\t' || input[i] == '\a')
        {
            i++;
        }

        if (input[i] == '\0') break;

        token = &input[i];

        while (input[i] != ' ' && input[i] != '\n' && input[i] != '\r' && input[i] != '\t' && input[i] != '\a' && input[i] != '\0')
        {
            i++;
            token_length++;
        }

        // Check if the token array has enough space
        if(position >= buffer_max - 1)
        {
            fprintf(stderr, "Too many command tokens\n");
            free_tokens(tokens);
            return NULL;
        }

        tokens[position] = malloc((token_length +1)*sizeof(char));

        if (!tokens[position]){
                perror("Memory allocation failed");
                free_tokens(tokens);
                exit(EXIT_FAILURE);
        }

        // Copy the characters into the allocated memory
        for(size_t j = 0; j<token_length;j++)
        {
            tokens[position][j] = token[j];
        }

        tokens[position][token_length] = '\0'; // Null-terminate the string
        position++; // Move to the next token position
        tokens[position] = NULL; // Keep the array NULL-terminated
        token_length = 0; // Reset token length for the next token

        if(input[i] == '\0') break; // Stop after processing the last token

    }

    tokens[position] = NULL; // Null-terminate the array of tokens
    return tokens;
}

void free_tokens(char** tokens){

    if(!tokens) return;

    for (size_t i=0; tokens[i]; i++)
    {
        free(tokens[i]); // releases the memory of each individual string.
    }

    free(tokens); // releases the array containing the pointers.
}
