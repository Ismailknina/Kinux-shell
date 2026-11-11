# TECHNICAL DOCUMENTATION

## 1.Understanding main.c

### 1.1 The main function :
#### 1.1.1. 'The main(int argc , char** argv , char** env)': 
    - We did put the int argc and char**argv even we dont need them because the main() have two forms to be written
      main(void) and main(int argc , char** argv). The Third parametre env The third parameter is a common Unix/Linux convention, 
      but it isn't guaranteed by ISO C
    - We can write also char *argv[] and char *env[] these just another syntax or way to type argc argv

#### 1.1.2. '(void)argc; (void)argv;':
    - Telling the Compiler that we dont need those 2 arguments Because compilers can generate warnings when function parameters are 
      declared but never used.

#### 1.1.3. 'run_shell(env)' : The main purpose of this function is to keep your shell running and waiting for commands.


### 1.2 The run_shell function :
#### 1.2.1. 'char* input = NULL;': 
    - Means that input will point to a sequence of characters representing the command entered by the user.
    - So for who don't know the difference between "char" and "char*" , char input; creates a variable that stores just one character.
      but char* input; allows you to work with an entire string.
      A shell needs to receive commands containing multiple characters, so a pointer to characters is appropriate.
    - NULL means that the pointer currently points to no valid object.

#### 1.2.2. 'size_t input_size = 0;':
    - "size_t" is a data type in C used to represent the size or number of objects in memory.
    - It is similare to write "int input_size;" But there is an important difference: 
      "int" can represent positive and negative integers and "size_t" represents non-negative integer values (0 and above).
    - "input_size" represents the allocated capacity, not necessarily the number of characters actually read.

#### 1.2.3. 'char** args;':
    - This is another pointer, but this time it points to pointers to characters.
    - For beginners PLZ Remeber : (char = one character) , (char* = a pointer to characters (commonly a string))
      and (char** = a pointer to pointers to characters (commonly an array of strings))
    - The shell need this Because commands contain multiple parts.
    - Important: args is not initialized in the code I provided. We'll come back to why that matters when discussing memory management.

#### 1.2.4. 'char* initial_directory = getcwd(NULL, 0);':
    - "getcwd()" means Get Current Working Directory , It is a POSIX function , not a standard C function that obtains the absolute path 
       of the directory in which your program is currently running.
    - Some people dont get what is POSIX so Quickly POSIX is It is a family of standards that defines interfaces allowing programs to 
      interact with operating-system services in a portable way.
      For example, POSIX provides functions for:
        Working with files and directories.
        Creating and managing processes.
        Interacting with the operating system.
        Managing permissions.
        Handling input and output.
    - (getcwd(NULL, 0);) It asks the implementation to allocate enough memory for the path automatically.The returned pointer refers to 
       that allocated memory.
    - We need "initial_directory" because This can be useful when implementing commands such as cd, particularly if I wants to support 
      returning to the original directory.
    
#### 1.2.5. 'while (1)'/INstruction':
    - This is an infinite loop , Because 1 is always true . A shell needs this because it must continuously wait for commands.

#### 1.2.6. 'printf("[Kinux-Shell]>> ");':
    - This displays the Kinux shell's prompt , The user sees this and knows that the shell is ready to receive a command.

#### 1.2.7. 'if (getline(&input, &input_size, stdin) == -1)':
    - getline() is a function used in C to read an entire line of text from an input stream and store it in a dynamically allocated 
       buffer. It is useful when building our Kinux Shell because you don't know in advance how long the command entered by the user will be. getline() is a POSIX function, not a standard ISO C function. It is declared in <stdio.h> on systems that support it
    - getline() takes 3 Arguments , getline(Argument 1,Argument 2,Argument 3) lets understand each argument :
        - Argument 1 : It's Type is "char **" and the Purposr is an Address of a pointer to the buffer.
          so Why do we pass "&input" instead of "input"?  "input" is a pointer to a character , "&input" is the address of that pointer.
          getline() needs the address because it may need to modify the pointer itself
          SO [ for "char *input = NULL;" There is no allocated buffer yet , When you call: "getline(&input, &input_size, stdin);" getline() can allocate memory and update input to point to that memory.
          for more clarification {Before calling "getline()" "input = NULL"} {After calling "getline()" "input" points to allocated memory} {Allocated buffer : HI\n\0}]
        - Argument 2 : It's Type is "size_t *" and the Purposr is Address of a variable containing the buffer capacity.
          Why &input_size? , Because getline() needs to know the current capacity of the buffer and may update that capacity when it allocates or resizes the buffer.
        - Argument 3 : It's Type is "FILE *" and the purpose is telling getline() where to read the data from.
          Here, stdin means standard input, usually the keyboard.
    - getline() returns the number of characters read , Notice that the return type is ssize_t, not size_t.
      What happens if something goes wrong? getline() returns -1 if it encounters EOF before reading any characters or if an error occurs or Ctrl+D sends an end-of-file indication to the terminal's input stream when used at an empty prompt.
      It tells the shell that no more input is available.
      That's why its return type is ssize_t: unlike size_t, it can represent negative values.
      BTW "EOF" means end-of-file

#### 1.2.8. 'perror("getline"); ':
    - perror() is a standard C library function , Its job is to display an error message based on the current value of errno
      The string "getline" is simply a label added before the error description.

#### 1.2.9. ' args = split_commands(input); ':
    - split_commands() is a seprate function that I will write later , in another file 
      To know what she does , imagine the user type "ls -l /home\n" the shell needs to identify the individual parts
      args[0] = "ls" , args[1] = "-l" , args[2] = "/home" , args[3] = NULL
      It takes one string and creates an array of strings. This is called tokenization.
      That's what split_commands() does with more things than that.
      WE gonna define it like this (char** split_commands(char* input)) so the results is a char** to be as the type of args and the input is a char* as the type of "input"


### 1.3 The built_in_commands function :
The purpose is to determine whether the command entered by the user is a built-in command.
For example, when the user types cd, pwd, echo, or exit, your shell needs to recognize the command and handle it.
This function have three parameters:
 - char** args : Array of strings containing the command and its arguments.
 - char** env : Array of environment variables.
 - char* initial_directory : Pointer to the directory where the shell initially started.
 - strcmp() is a function that compare two strings , it a function imported from <strings.h> in the coming versions of this project if there is enough time I will write it my self
 - Why not simply write: {if (args[0] == "cd")} 
 Because in C, strings are arrays of characters, and comparing two string pointers with == compares their memory addresses, not their textual contents.


#### 1.3.1. 'char* input = NULL;': 
-----------------------------------------------------------------------------------------------------------------------------------------


## 2.Understanding command_parser.c

For this implementation, spaces, tabs, and certain other whitespace characters separate tokens.
One limitation: this is a simple tokenizer, not a complete shell parser. It doesn't understand quotation marks, escaped spaces, pipes, or redirections yet.
### 2.1 The split_commands function : 
The objective of this function is it takes one string and creates an array of strings.
This is called tokenization.
A token is one individual component of the command.
#### 2.1.1. 'size_t buffer_size = MAX_INPUT_SIZE;':
    - We need buffer size Because the shell needs to allocate memory for the array that will contain the tokens.

#### 2.1.2. 'char** tokens = malloc(buffer_size * sizeof(char*));':
    - malloc() is a C function imported from <stdlib.h> It dynamically allocates a block of memory and returns a pointer to that memory ? It does NOT automatically allocate memory for the strings those pointers will eventually point to.
    We'll allocate each string separately later.
    - Why 'char*' inside sizeof() rather than 'char'? I did that because 'tokens' is an array of pointers to strings . I need space for 1024 pointers , not 1024 individual characters. So at this stage I allocated memory for the pointers, not yet for individual strings.

#### 2.1.3. 'char* token = NULL;':
    - The Pointer here temporarily points to the begining of ethe current token insude the original input string , It does not allocate another string. It simply points into the existing 'input' buffer.
    - In abother way , This is a pointer to a character. Its job is to point to the beginning of the current word being extracted from the original input.'token' does not contain a copy of the word. It stores the address of the first character of that word in the original input.
    - For more Clarification : 
        input  = Points to the complete original command
        token  = Temporarily points to the beginning of 1 token
        tokens = Points to the array containing the resulting strings
    - I initialize it to NULL , Because before the parser finds a token, it doesn't point to a valid character.

#### 2.1.4. 'size_t position = 0;':
    - This variable tells the program where to store the next token in the tokens array.
    The variable position determines which index is currently being filled. After storing the first token I need to increase the position .

#### 2.1.5. 'size_t token_length = 0;':
    - This variable counts the number of characters in the current token. we do need it cuz I must know how much memory to allocate for each individual token.

#### 2.1.6. 'if (!tokens) {perror("Malloc");exit(EXIT_FAILURE);}':
    - Here simply we know that malloc() can fail maybe the system might not have enough available memory , and when malloc() fials it returns 'NULL'.
    - 'if (!tokens)' <=> 'if (tokens == NULL)'
    - the exit() terminates the entire program.
    - "EXIT_FAILURE" is a standard macro from stdlib.h indicating unsuccessful program termination.

#### 2.1.7. 'for (size_t i = 0; input[i]; i++)':
    - This is where the actual parsing happens.
    - We start at index 0 and continue as long as the current character is not '\0'. 'i' is the index used to navigate through the original input string.

#### 2.1.8. 'while(input[i] == ...)':
    - We need this cuz We don't want to create empty tokens for spaces. So this loop advances i until it reaches a character that isn't one of the specified whitespace characters.

#### 2.1.9. 'token = &input[i];':
    - let's say the command that we are passing is  'ls -l /home' , and suppose that 'i=3' , which means we are curently in '-' , so 
    tpken = input[3]. input[3] gives the character '-' and the &input[3] gives the address of the character so finally token stores that adress that means 'token' points to the beginning of the word '-l'
    - who is reading this Documentation remember that We haven't copied anything yet , 'token' is just a pointer into the original input string.

#### 2.1.10. 'while(input[i] != ...)':
    - Here this while job is to know when we have to stop the word like we passed 'ls -l /home' it has to take like the last exemple '-l' not '-l ' when the programme see a space or something stop
    - Every time we encounter a character We increase the length of the current word. using {token_length++;}
    -- Know we know Where the word starts: 'token' , and How many characters it contains: 'token_lengthe'

#### 2.1.11. 'tokens[position] = malloc((token_length + 1) * sizeof(char));':
This part of code , Allocate memory for the current word
    - We do add 1 Because we need  positions for the actual characters and one additional position for the null terminator '\0'
    - As we know before 'tokens[position]' is a 'char*' , So we are allocating memory for a string and storing the address of that allocated memory inside the corresponding pointer slot.

#### 2.1.12. 'for (size_t j = 0; j < token_length; j++)':
This part of code Copy the characters into the allocated memory
    - let's see '/home' : 
    we have 'token' points to input[6], the beginning of "/home"
    'tokens[2]'points to newly allocated memory Then : 
    tokens[2][0] = token[0];  // '/'
    tokens[2][1] = token[1];  // 'h'
    tokens[2][2] = token[2];  // 'o'
    tokens[2][3] = token[3];  // 'm'
    tokens[2][4] = token[4];  // 'e'
    - Now '/home' exists in two places :
    Inside 'input' and Inside the newly allocated memory referenced by 'tokens[2]'.

#### 2.1.13. 'tokens[position] = NULL;':
    - Suppose we processed: ls -l /home
    We now have:tokens[0] = "ls"
                tokens[1] = "-l"
                tokens[2] = "/home"
    But how does another function know when the array ends? We add:
    tokens[3] = NULL; This is called a null-pointer terminator.
    This is important because functions such as execvp() expect an array of string pointers terminated by NULL.


### 2.2 The free_tokens function : 
The idea is to release all the memory allocated by split_commands().
we allocated memory in two different places:
{char** tokens = malloc(buffer_size * sizeof(char*));}
{tokens[position] = malloc((token_length + 1) * sizeof(char));}

    - '!tokens' means the pointer is NULL We want to return only when there is nothing to free
    - free(tokens[i]) releases the memory of each individual string.
    - free(tokens) releases the array containing the pointers.
    - I must release the individual strings before releasing the array because you need the array to access their addresses.


## 3.Understanding Makefile
- A 'Makefile' is a file that tells a build tool called "make" how to compile your project.
- we do have 'main.c' , 'command_parser.c' , 'built_in_commands.c' ,   'shell_config.h' , 'Makefile'
Without a Makefile, I might compile the project manually like this : gcc main.c command_parser.c built_in_commands.c  -o Kinux
As your project grows, this command can become longer and more complicated.
- A 'Makefile' allows me to save the compilation instructions once and execute them whenever I need. So with the 'Makefile' I can simply type "make" , And "make" reads the 'Makefile 'and executes the necessary commands.
- TARGET, OBJ, CFLAGS, and CC are all variables that you can define and use in my Makefile they are conventional names commonly Make doesn't require you to use those exact names.
Using conventional names makes your Makefile easier for other developers to understand
- "all", "clean", "fclean", and "re" are called targets . Target tells Make what task you want it to perform ,The command underneath it is the action Make performs.

Know let's see the : "rm -f *.o" , "rm -f $(TARGET)" , "fclean all"
- But before let's understande what is '.o file':
The '.o' extension means object file , When you compile a C program, the compilation process can produce intermediate files called object files.The object files contain compiled machine-code information, but they are not yet the final executable.The linker combines the object files to produce our executable. 

- Now good let's see the commands , 
"rm" ==> Remove files , "-f" ==> Force removal without confirmation; don't complain if files don't exist , "*.o" ==> Match filenames ending in .o
There is a question maybe someone asks , Why remove object files?
>> Because they are generated intermediate files. You can regenerate them from your .c files whenever needed.

- Why do we have "fclean"? The name "fclean" conventionally means full clean. "fclean" removes object files AND the final executable.
Why would you want to remove the executable too?
>> Imagine you changed your source code and want to rebuild the entire project from scratch. You don't want an old executable sitting there and accidentally running it."fclean" gives you a completely clean build directory, except for your source and other files that aren't generated by the build.


- Why do we have re: fclean all? "re" conventionally means rebuild.
"re: fclean all" == > This tells Make that the re target depends on fclean and all.

Finally : The main idea: clean removes intermediate files, fclean removes all generated build files, and re creates a fresh build.



## 4.Understanding shell_config.h
So first we know that '.h' stands for "header" and a header file generally contains declarations that tell other source files what functions, types, macros, and other elements are available.

So for exemple in 'main.c' to use the split-commands() main needs to know that split-commands() exists and what its return type and parameters are. That's where the header file comes in.

What does #ifndef, #define, and #endif mean? :
These three directives are called header guards. They prevent the contents of shell_config.h from being processed repeatedly within the same .c file.
- #ifndef SHELL_CONFIG_H : If SHELL_CONFIG_H has NOT been defined, continue processing the code.
- #define SHELL_CONFIG_H : Define the identifier SHELL_CONFIG_H. Now the condition is no longer true if the same header is included again.
- #endif : It marks the end of the conditional block.

## 5.Understanding built_in_commands.c
This filr implements some of the built-in commands implemented directly insiade the shell program , such as "cd","pwd","echo" and "env"

### 5.1. command_cd() :
This command means change directory , it changes the curent working directory . The function that it is responsible for this operation is 'chdir()' , it is a POSIX function declared in <unistd.h> (That's why I declare it in <shell_config.h>)

So Now first we have to know this function so we can use it , Its prototypr is : 'int chdir(const char *path);' easily this function receives a path and attempts to change the current working directory to that path.
It returns 0 ==> 'Success' and -1 ==> 'Failure'

#### 5.1.1. int command_cd(char** args, char* initial_path) :
There are two arguments:
    - "char** args" This is the array od strings returned by the 'split_commands()' function  
    [Suppose the user enters "cd /home/user" the parser give us args[0]="cd" , args[1]="/home/user" and args[2]="NULL"]
    - "char* initial_path" this contains the initial directory of my shell.

#### 5.1.2. if (args[1] == NULL) :
This checks whether the user provided a path.

#### 5.1.3. else if (chdir(args[1]) == 0) :
Let's say the user enters: "cd /tmp" My program calls "chdir("/tmp")" , If the directory exists and the process has the necessary permissions, "chdir()" returns 0. so the condition insied "elseif" becomes true. Currently, nothing happens inside because the success message is commented out.But the directory has already changed!

Important question may comes to someone reading this like me when I was studying for this project is : Why does "chdir()" need to be called inside my shell process ?
>> Imagine the shell launched a separate child process just to execute "cd". That child process could change its own working directory, but when it terminates, the original shell would remain in its original directory.That's why "cd" must be implemented as a built-in command.

### 5.2. command_pwd() :
This command means print working directory ,Its purpose is to display the current directory of your shell.
The function that it is responsible for this operation is 'getcwd()' , it is a POSIX function declared in <unistd.h> (That's why I declare it in <shell_config.h>)

Its protoype is "char *getcwd(char *buf, size_t size);"
It obtains the absolute path of the current working directory.

#### 5.2.1. char* cwd = NULL; :
Here I declared a pointer to a charachter This one will contain the adress of a string representing the current working directory.
And I initialize it with "NULL" because it is a good practice or habit to initilize pointers before assingning them a valid address

#### 5.2.2. cwd = getcwd(NULL, 0); :
Normally "getcwd()" receives a buffer where it will store the path as we did before in main (if I remeber)
like for example I have to do :
"char buffer[1024];
getcwd(buffer, sizeof(buffer));"
but here I use "getcwd(NULL,0)"
The idea is to ask the implementation to allocate enough memory dynamically for the path. The returned pointer refers to dynamically allocated memory. 

Important thing to talk about is that Because memory was dynamically allocated, I am responsible for releasing it using "free()".

#### 5.2.3. if (cwd != NULL) : 
As we know if "getcwd()" succeeds , it returns a valid pointer. If it fails,it returns "NULL"
So if the operation succeeded ==> "printf("%s\n", cwd);"

#### 5.2.4. free(cwd) : 
Remeber I just talked abvout "cwd = getcwd(NULL,0)" it allocated memory dynamically. When I finish using that memory, I should release it. Otherwise, repeated calls to pwd could accumulate memory that my program no longer needs.
This is called a 'memory leak'.

### 5.3. command_echo() :
"echo" displays text in the terminal.
This function handels arguments and environment variables that's why the parameters are (char** args, char** env)

#### 5.3.1. int new_line = 1; : 
This variable controls whether the function prints a newline , I initialize it to 1 Because the default of "echo" is to end the output with a newline , so The next shell prompt should appear on a new line. And the "-n" means don't add a newline.
So new_line = 1 : newline enabled
new_line = 0 : newline disabled

#### 5.3.2. size_t i = 1; : 
This variable represents the index of the argument currently being proccessed , and I start it with 1 cuz the 0 index is for the command "echo"

#### 5.3.3. if (args[1] != NULL && my_strcmp(args[1], "-n") == 0) : 
This condition has two parts :
    - First : "args[i] != NULL" : This one checks is the argument exist after "echo" cuz we cannot safely compare an argument that doesn't exist
    - Second : strcmp(args[1],"-n")== 0 :
    Checks whether the first argument is exactly "-n".
if the condition True then "new_line = 0;" and "i++" for moving the argument from 1 to 2.
Because args[1] contains "-n", and we don't want to print "-n" as ordinary text.

#### 5.3.4. for (; args[i]; i++) : 
For a traditionnal "for" loop it looks like this "for (initialization; condition; increment)" 
Here is not the same , The initialization part is empty because i was already initialized above.

#### 5.3.5. if (args[i][0] == '$') : Checking whether an argument starts with $
First let's understand "args[i][0]" It accesses the first character of the current argument. Suppose "args[i] = "$PATH";" so "args[i][0] == '$'" is True Otherwise False if it doesn't starts with "$"

#### 5.3.6. char* value = getenv(args[i] + 1); : 
LOOKE, here I will use an exemple to explain better : 
imagine "args[i]= "$PATH"" the "args[i]" points to the first character which is "$" Soooooo "args[i] + 1" points to letter "P"
I remove the "$" Because the environment variable is stored under the name "PATH", not "$PATH". So the funtion calls "getenv("PATH")"

#### 5.3.7. if (value) {printf("%s",env_var)} : 
If getenv() returns a valid pointer, the condition is true.
The function prints the variable's value.
If the variable isn't found, my function prints "Not Found".

#### 5.3.8. Understanding the spaces and newline logic in echo : 
The main idea is to :
-- Print a space between arguments.
-- Print a newline after the last argument unless "-n" was specidied

Exemple :: echo Hello world :: 

In the First "i=1;" and "new_line = 1"; For the first iteration "args[1] = "Hello"" So the Function will prints (Hello) , Next :  "args[2] != NULL" which is True because the word "world" exists , So it executes {"new_line = 0;"}{"printf(" ");"} The output is "Hello" ; Then: {"if (new_line)"} is false, so no newline is printed. Finally: new_line = 1;
Second iteration: args[2] = "World"; The function prints:"World" ; Then: {"args[3] != NULL"} is false because args[3] is NULL. So no space is printed. new_line is still 1, so: printf("\n") is executed.
Final output: "Hello World"

### 5.4. command_env() :
The "env" command displays environment variables.
For example, your environment might contain:
    HOME=/home/user
    PATH=/usr/bin:/bin
    USER=user
    SHELL=/bin/bash
Each environment variable is represented as a string.
We have "int command_env(char** env)" , Here "env" is an array of pointers to strings. 
So:
{"env[0]-->"HOME=/home/user""}
{"env[1]-->"PATH=/usr/bin:bin""} etc

### 5.5. char* find_command_in_path(const char* command, char** env); :
This is a forward declaration of the function cuz we will need it in "command_which()"
The function returns a pointer to a character, usually the beginning of a string.
Why return char* instead of int ??
Because we do,'t just to know whether the command exists or not , we want to retrieve its complete path.
This function for example might receive " command = 'gcc' " and eventually return a dynamically allocated string "/usr/bin/gcc" or it returns "NULL" if it cannot find the command

### 5.6. command_which() :
The general idea is to search each directory until the command is found.

#### 5.6.1 const char* built_in_commands[] = ... :
This is an array of pointers to constant characters , we do "const char* " So each element points to a string  , "const" means you shouldn't modify the characters through these pointers; For example "built_in_commands[0][0] ='c' " this would be invalid , We only need to read and compare these strings , so making them "const" is appropriate .

#### 5.6.2 char* full_path = find_command_in_path(args[1], env);
If the loop doesn't find a built-in command, execution reaches this line.
For example we try "which gcc" here gcc isn't in my built-in list so my shell will calls "find_command_in_path("gcc", env);" and The returned value is stored in : "char* full_path;" maybe It might point to : "/usr/bin/gcc" Or it might be NULL if the command wasn't found.
Then we move to the place if the command is found

### 5.7. find_command_in_path() :
This the core of my implementation of "which".
Its JOB is : Finding the "PATH" environment variable , extract its value , separate the directories and then construct possible path for the requested command then checkin if that path is executable and finally returns the path if found.

#### 5.7.1 Declaring the variables :
- "path_env" ==> Points to the original PATH value inside the environment array.
- "path_copy" ==> Holds a separate, modifiable copy of PATH.

Why do we need both "path_env" and "path_copy" ?
Because you'll use a tokenization function to split PATH.
Many tokenization functions modify the string they're processing. You generally don't want to modify the original environment string.

- "token" ==> Points to the current directory extracted from PATH.
- "full_path" ==> A local character array used to construct a candidate executable path.

#### 5.7.1 Finding PATH inside the environment :
Your environment is an array of strings, for example: 
"env[0] = HOME=/home/user
env[1] = USER=user
env[2] = PATH=/usr/bin:/bin" You don't necessarily know which index contains PATH.

The "strncmp()" This compares only the first five characters of the strings. ( I used "strncmp" not "strcmp" there is a difference)

#### 5.7.2 if PATH doesn't :
"if (path_env == NULL) return NULL;"
If the loop finishes without finding "PATH", "path_env" remains NULL. The function returns NULL to indicate that it couldn't perform the search. The caller,"command_which()", will then report that the command wasn't found.

#### 5.7.3 Duplicating PATH :
This is an important memory-management concept.
"strdup()" it allocates enough memory for a copy of a string , copies the string , and returns a pointer to the copy.

Why do we need a copy ?
Because of the next function: "strtok()" will likely modify the string by replacing delimiters with null terminators. We don't want to modify the original environment.

#### 5.7.4 Splitting PATH into directories :
Our "PATH" contains multiple directories separated by colons.
for example : "/usr/local/bin:/usr/bin:/bin" , The colon ":" separates the directory names, and we want to obtain these individual strings : "/usr/local/bin" "/usr/local/bin" "/bin"

#### 5.7.5 Constructing the complete path :
- Inside the loop: "size_t len = strlen(token);" I calculate the length of the current directory .
Why do we need its length ?
Because I want to check whether the directory already ends with "/".

- see this code :[  "if (token[len - 1] != '/') {
    snprintf(full_path, sizeof(full_path), "%s%s%s",
             token, "/", command);
} else {
    snprintf(full_path, sizeof(full_path), "%s%s",
             token, command);
}"  ]
Suppose "token = "/usr/bin" ;" and "command = "gcc";" The directory doesn't end with "/"".
So the line "snprintf(full_path, sizeof(full_path), "%s%s%s", token, "/", command);" will constructs : "/usr/bin/gcc"

- There is a question what's "sprintf()" and "snprintf()" ?
They are similar to "printf()", but instead of displaying the formatted result in the terminal, they store it inside a character array (string). The only difference between those two is :  "snprintf()" lets you specify the maximum size of the destination buffer.

#### 5.7.6 Checking whether the executable exists and can be executed :
- Here , This is where my shell asks the operating system about the candidate path .
The "access()" is a POSIX function declared in <unistd.h> its general form is : "int access(const char *pathname, int mode);" It checks whether the calling process has the requested access to a file. The argument "X_OK" means that you're checking execute permission.If the check succeeds, it returns 0 .

- Why do we use X_OK ?
Because "which" is searching for executable commands, not just any file.

- Why free "path_copy" before returning ?
Remember that I created "path_copy" with "strdup()". no longer need it once I have found the command.

- Why return "strdup(full_path)" ?
It is stored as an automatic local variable. Its lifetime ends when the function returns.
Therefore, I must not return a pointer directly to that local array.
This would be wrong: "return full_path;"
Instead, I make a dynamically allocated copy: "return my_strdup(full_path);"
That returned string remains valid after the function returns, until the caller frees it.
That's why "command_which()" later does: "free(full_path);"

### 5.8. count_env_vars() :
- This is a helper function , it is simple, but it's used by both "setenv" and "unsetenv". I need it cuz i nned to know how many actual environment variables exist before allocating a new array.

### 5.9. Understanding command_setenv() :
- Its purpose is to add an environment variable in two forms (VARIABLE=VALUE) or (VARIABLE value)

#### 5.9.1 Counting the current environment variables :
This calculates the number of existing variables.
I'll use this number to calculate how much memory to allocate.

#### 5.9.2. Allocating the new environment array :
- "char** new_env = malloc((env_count + 2) * sizeof(char*));"

- Let's examine this carefully.
Why "char**" ? Because "new_env" is an array of pointers to strings.
Why "env_count + 2" ? Cuz I want to keep all existing envs , add a new one and at the end add a "NULL" pointer

- "sizeof(char*)" give us the size of 1 pointer
The allocation reserves enough memory for five pointers, not five complete strings.

!! Important distinction: malloc() here allocates the array of pointers. The strings themselves are allocated separately later.

#### 5.9.3. Copying the existing environment variables :
- Why copy the existing variables ?
Because I am building a new environment array that includes the old variables plus the new one.
I don't want to lose the existing variables when adding one.

- Why check strdup() ?
Memory allocation can fail.
If copying one environment variable fails, the function must clean up the memory it already allocated.
Let's say it successfully copies three variables, but allocation for the fourth fails. Then it frees the first three copied strings .
And then frees the array itself.

- Why does it not free env[j] ?
Because those strings belong to the original environment, not to the copies it just allocated.

#### 5.9.4. Supporting two input formats :
The function supports two formats.

- Format A: setenv VAR=value : 
User enters: setenv COLOR=blue
The parser produces: "args[0] = "setenv";" "args[1] = "COLOR=blue";"
"args[2] = NULL;" Therefore  "args[2] == NULL" is True so the code  duplicates the complete string "COLOR=blue"

- Format B: setenv VAR value :
User enters: setenv COLOR blue
The parser produces: "args[0] = "setenv";" "args[1] = "COLOR";" "args[2] = "blue";" "args[3] = NULL;"
Now args[2] == NULL is false , so the code creates a new string.

#### 5.9.5. Checking whether the new variable was created :
This is another allocation-failure check.
If "new_var" is NULL, the function cannot complete the operation.
It frees the strings copied into "new_env", then frees the pointer array, and returns the original environment.

#### 5.9.6. Adding the new variable :
" new_env[env_count] = new_var;
new_env[env_count + 1] = NULL; "
This is where the new environment array becomes complete.