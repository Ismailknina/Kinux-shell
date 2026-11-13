# Kinux-Shell — Technical Documentation

**Contents**

0. About this document
1. Project overview — 1.1 What Kinux-Shell is · 1.2 Building and running
2. Architecture — 2.1 Source files and responsibilities · 2.2 The command-execution lifecycle · 2.3 The shell's private environment vs. the process environment
3. shell_config.h — the shared header
4. main.c — entry point and the read–parse–execute loop
5. command_parser.c — tokenization
6. built_in_commands.c — built-in commands
7. external_commands.c — process creation and execution
8. The Makefile
9. Memory-management summary
10. Known limitations and future improvements

Appendix A. POSIX vs. ISO C functions used · Appendix B. Glossary

## 0. About this document

**Purpose.** This document explains how Kinux-Shell is implemented: what every important function does, how the source files interact, and why specific design decisions were made. It serves two audiences:

- A technical reader — a recruiter, developer, or interviewer — who wants to verify that I understand the C and POSIX concepts behind my own code.
- A reader with basic C knowledge who has never implemented a shell and wants to learn how one works.

**Scope.** Kinux-Shell is a small, single-process, foreground Unix-like shell. It reads a line of input, splits it into tokens, runs built-in commands inside the shell process, and launches external programs in child processes. It does not implement scripting, pipes, redirection, quoting, job control, or signal handling; Section 10 states the limitations explicitly. Everything described here matches the source code as of this writing — where a behavior looks like a bug or a gap, I say so rather than hiding it.

**Conventions.**

- I write in the first person, because this document is about my implementation and my decisions.
- Identifiers are formatted as code (`malloc`, `char **env`), and explanations are anchored to the source with references such as `main.c:78`.
- A "POSIX function" is a function standardized by POSIX but not part of ISO C. Appendix A lists which ones this project uses.

## 1. Project overview

### 1.1 What Kinux-Shell is

Kinux-Shell is a command interpreter written in C. It displays a prompt (`[Kinux-Shell]>> `), reads commands from standard input, and executes them. Its behavior is deliberately a minimal subset of a classic Unix shell:

- **Built-in commands** run inside the shell process itself: `cd`, `pwd`, `echo`, `env`, `setenv`, `which`, `.help` (and `help`), plus `exit`/`quit`.
- **External commands** (`ls`, `gcc`, `./myprog`, …) run in child processes created with `fork()` and replaced with `execve()`, located by searching `PATH`.
- The shell maintains its **own view of the environment** that `setenv` can modify; external programs receive that view, not a stale system snapshot.

| Feature | Status |
|---|---|
| Prompt and line reading with `getline()` | Implemented |
| Whitespace tokenizer | Implemented — no quoting, pipes, or redirection |
| Built-ins: `cd`, `pwd`, `echo`, `env`, `which`, `.help`/`help`, `exit`/`quit` | Implemented |
| `setenv` (both `VAR=value` and `VAR value` forms) | Implemented — appends; does not replace existing variables |
| `unsetenv` | Implemented as a function, but **not yet reachable** from the dispatcher (Section 6.7) |
| External command execution with `PATH` search | Implemented |
| Signal handling, job control, `$?`, pipes, redirection, quoting, scripting | Not implemented |

### 1.2 Building and running

The project builds with `make` (the Makefile is explained in Section 8):

```
$ make      # compiles the four .c files and links the executable
$ ./Kinux   # starts the shell
```

| Target | Effect |
|---|---|
| `make` (`all`) | Builds the executable `Kinux` |
| `make clean` | Removes object files |
| `make fclean` | Removes object files and the executable |
| `make re` | Full rebuild (`fclean` followed by `all`) |

Available commands, as printed by `.help` (`main.c:3–15`):

| Command | Behavior |
|---|---|
| `cd <directory>` | Changes the working directory with `chdir()` and prints a confirmation; `cd` without an argument only prints an error |
| `pwd` | Prints the working directory via `getcwd()` |
| `echo [-n] text...` | Prints its arguments; `-n` as the first argument suppresses the trailing newline; `$VAR` expands from the shell's environment |
| `env` | Lists the shell's environment variables |
| `setenv VAR=value` or `setenv VAR value` | Adds a variable to the shell's environment |
| `unsetenv <variable>` | Implemented, but currently **not dispatched** — typing it attempts to run an external program and fails (Section 6.7) |
| `which <command>` | Reports whether a command is built-in, or prints the full path found in `PATH` |
| `.help` / `help` | Prints the command list |
| `exit` / `quit` | Terminates the shell |

## 2. Architecture

### 2.1 Source files and responsibilities

| File | Responsibility |
|---|---|
| `shell_config.h` | The single shared header: standard includes, the `MAX_INPUT_SIZE` macro, and prototypes for every function used across translation units (Section 3) |
| `main.c` | Entry point, the read–parse–execute loop (`run_shell`), the built-in dispatcher (`built_in_commands`), and the help text (`display_help`) (Section 4) |
| `command_parser.c` | Tokenization: `split_commands` turns the input line into a NULL-terminated array of strings; `free_tokens` releases it (Section 5) |
| `built_in_commands.c` | Built-in command implementations (`cd`, `pwd`, `echo`, `env`, `which`, `setenv`, `unsetenv`) plus the `PATH` search used by `which` (`find_command_in_path`) (Section 6) |
| `external_commands.c` | Process creation and execution: `executors` (fork and wait), `child_process` (`PATH` search plus `execve`), and the `PATH` helpers `get_path` and `split_paths` (Section 7) |
| `Makefile` | Build automation (Section 8) |

The dependency direction is one-way: `main.c` calls the parser and the dispatcher; the dispatcher calls built-in commands or the executor; the executor uses the `PATH` helpers. Every cross-file call relies on the prototypes declared in `shell_config.h`.

### 2.2 The command-execution lifecycle

For a command such as `ls -l /home`, from one prompt to the next:

```
1.  printf("[Kinux-Shell]>> ")                        main.c:76
2.  getline(&input, &input_size, stdin)               main.c:78
      → stores "ls -l /home\n" in a heap buffer
        that is reused across loop iterations
3.  args = split_commands(input)                      main.c:85
      → { "ls", "-l", "/home", NULL }                 command_parser.c:3
4.  if (args && args[0])                              main.c:87
      built_in_commands(args, &env, initial_dir)      main.c:89
5.  Dispatcher compares args[0] against built-ins     main.c:18
      ├─ built-in       → run it inside the shell process
      │                   ("cd", "pwd", "echo", "env", "which", ".help", …)
      ├─ "setenv"       → swap the environment array: *env = command_setenv(...)
      ├─ "exit"/"quit"  → exit(EXIT_SUCCESS)
      └─ otherwise      → executors(args, *env)       external_commands.c:3
6.  executors():
      ├─ child  → child_process():                    external_commands.c:52
      │     if args[0] contains '/', execve(args[0], args, env) directly
      │     else search each PATH directory and execve("/usr/bin/ls", args, env)
      │     (a successful execve never returns — the child *becomes* ls)
      └─ parent → waitpid(pid, &status, 0)            external_commands.c:31
            → collects the child's exit status or termination signal
7.  free_tokens(args) — on every iteration            main.c:92
8.  Back to step 1.
```

Two points are worth internalizing early, because they shape everything that follows:

- **Parsing and execution are separate stages.** `split_commands()` only produces the token array; it never executes anything. That separation is what allows the dispatcher to treat built-in commands and external programs uniformly.
- **One process, except when necessary.** Built-in commands run in the shell process; only external programs justify `fork()`. Real shells follow the same rule — and Section 6.1 explains why `cd` cannot work any other way.

### 2.3 The shell's private environment vs. the process environment

This is the most important design idea in the project. It explains several choices that would otherwise look arbitrary: why `echo $VAR` does not call `getenv()`, why `setenv` rebuilds the whole array, and why `execve()` is handed `env` explicitly.

- ISO C defines `main()` with either zero or two parameters; the third parameter, `env`, is a widespread Unix convention but not guaranteed by the standard (`main.c:100`). It gives the program the **process environment**: a NULL-terminated array of `"NAME=value"` strings.
- At startup, my `env` parameter points at the **same array the C library maintains (`environ`)**. Those strings are owned by the system — I must never free them.
- `command_setenv()` cannot safely append to that system-owned array, so it builds a **new, heap-allocated array** containing duplicates of every existing entry plus the new variable, and the dispatcher swaps it in: `*env = command_setenv(args, *env)` (`main.c:38`). From that moment on, the shell works with a private, modifiable environment.
- Consequence: after the first `setenv`, the process environment (`environ`, which is what `getenv()` reads) and the shell's private array **diverge**. `getenv("COLOR")` would consult the stale system copy and miss variables I just set. This is exactly why `echo $VAR` scans the shell's own array manually (Section 6.4) and why `execve()` must be passed `env` explicitly — a child program launched with the implicit environment would not see my variables either.
- The flip side is an ownership problem: the shell's environment mixes system-owned strings (before any `setenv`) and heap-owned copies (after). There is no safe way to free the old array wholesale, so the previous environment is deliberately **abandoned** (a bounded leak) whenever `setenv` succeeds. Section 6.7 and Section 9 cover this in full.

An alternative worth mentioning: POSIX also provides `setenv(3)` and `getenv(3)`, which read and modify the real process environment. I chose explicit arrays instead, for three reasons. First, `execve()` requires an explicit `envp` argument anyway, so the shell must be able to produce a complete environment array regardless. Second, passing `env` through the call chain makes the data flow visible and testable — every function that needs the environment receives it as a parameter. Third, it gives me full control over parsing (`setenv` accepts two input formats) and memory, instead of relying on library behavior I do not manage.

## 3. shell_config.h — the shared header

### 3.1 What the header is for

A header file (`.h`) contains declarations that make functions, types, and macros available to several source files. The distinction that matters here:

- A **declaration** (prototype) tells the compiler a function's name, return type, and parameter types.
- A **definition** is the actual implementation.

Definitions live in the `.c` files; `shell_config.h` holds only declarations, plus the shared includes and one macro. When `main.c` calls `split_commands(input)`, the compiler must already know that the function exists and returns `char **` — without the prototype, an old C compiler would assume it returns `int`, and the mistake would surface as garbage at link time or run time. With the prototype visible, every translation unit that includes the header sees the same contract, the compiler verifies each call against it, and the linker resolves the call to the definition in `command_parser.c`.

### 3.2 Header guards

```c
#ifndef SHELL_CONFIG_H   /* if not defined yet, process what follows */
#define SHELL_CONFIG_H   /* mark it as seen */
/* ... contents ... */
#endif                   /* end of the conditional block */
```

On the first inclusion, `SHELL_CONFIG_H` is undefined, so the contents are processed and the macro is defined. On any later inclusion within the same translation unit, the `#ifndef` fails and the contents are skipped. Repeating identical prototypes is harmless in C, but the guard is essential defensive practice: `MAX_INPUT_SIZE` must not be redefined, and the guard keeps the header idempotent no matter how include chains grow. The alternative, `#pragma once`, is widely supported but not standardized; I use the classic guard.

### 3.3 The includes

| Header | Provides | Used for |
|---|---|---|
| `<stdio.h>` | `printf`, `perror`, `getline`, `stdin`, `FILE` | Input, output, error reporting |
| `<stdlib.h>` | `malloc`, `free`, `exit`, `EXIT_SUCCESS`, `EXIT_FAILURE` | Dynamic memory and termination |
| `<string.h>` | `strcmp`, `strncmp`, `strlen`, `strdup`, `strchr` | String comparison and manipulation |
| `<unistd.h>` | `getcwd`, `chdir`, `access`, `fork`, `execve`, `_exit`, `X_OK` | POSIX operating-system services |
| `<stddef.h>` | `size_t`, `NULL` | Sizes and the null pointer constant |
| `<sys/types.h>` | `pid_t` | Process identifiers |
| `<sys/wait.h>` | `waitpid`, `WIFEXITED`, `WIFSIGNALED`, `WEXITSTATUS`, `WTERMSIG` | Waiting for children and decoding their status |

**Portability note.** The project targets POSIX systems (I developed and tested it on Ubuntu). The comment at `shell_config.h:17` records a real friction point: `<sys/wait.h>` was not available in my MSYS2 UCRT64 environment on Windows. Also note that `getline` and `strdup` are POSIX functions, not ISO C — they compile here because glibc declares them by default, but a strict `-std=c99 -pedantic` build could require feature-test macros.

### 3.4 MAX_INPUT_SIZE

```c
#define MAX_INPUT_SIZE 1024
```

This is an object-like macro: the preprocessor replaces every occurrence of `MAX_INPUT_SIZE` with `1024` before compilation. It is not a variable — it occupies no memory and has no type. I use it in two different roles:

1. As the **capacity of the token-pointer array** in `split_commands()` — 1024 slots for `char *` values, which caps the command at roughly 1023 tokens (Section 5.2).
2. As the **size of stack buffers** such as `full_path` in `child_process()` (`external_commands.c:77`).

Using one constant for two different concepts (token count and path length) is slightly conflated; separating them into `MAX_TOKENS` and a path-buffer constant would be cleaner (Section 10).

### 3.5 Function prototypes

The header groups the prototypes by subsystem, which doubles as a map of the project:

| Group | Prototypes |
|---|---|
| Parser (`command_parser.c`) | `split_commands`, `free_tokens` |
| Built-in commands (`built_in_commands.c`) | `command_cd`, `command_pwd`, `command_echo`, `command_env`, `command_which`, `command_setenv`, `command_unsetenv` |
| Executor (`external_commands.c`) | `executors`, `child_process` |
| Path helpers (`external_commands.c`) | `get_path`, `split_paths` |
| Dispatcher (`main.c`) | `built_in_commands` |

One function is deliberately absent: `find_command_in_path` is declared as a **forward declaration** inside `built_in_commands.c:93`, because `command_which` calls it before its definition appears later in the same file. That works, but moving the prototype into the header would be more consistent with everything else — a small improvement I list in Section 10.

## 4. main.c — entry point and the read–parse–execute loop

### 4.1 main() and its three parameters

```c
int main(int argc, char **argv, char **env)
```

ISO C only defines `main` with zero parameters (`main(void)`) or two (`main(int argc, char **argv)`). The third parameter, `env`, is a widespread Unix convention — the process environment array — but it is not guaranteed by the standard; the portable POSIX way to obtain it is the global `environ`. I use the three-parameter form because it hands me the environment directly, and Section 2.3 builds the whole design on it.

The types deserve a note: `char *argv[]` and `char **argv` are the same type in a parameter list (an array parameter is adjusted to a pointer), so the two notations are interchangeable.

```c
(void)argc;
(void)argv;
```

Compilers warn about parameters that are declared but never used (`-Wextra` reports them). Casting each unused parameter to `void` is the idiomatic way to say "I know, and I mean it" without weakening warnings elsewhere. If a later version starts using command-line arguments, the casts simply go away.

Finally, `run_shell(env)` passes the environment into the loop and never returns until the shell exits.

### 4.2 run_shell(): state and setup

```c
char  *input       = NULL;
size_t input_size  = 0;
char  **args;
char  *initial_directory = getcwd(NULL, 0);
```

- `input` and `input_size` are the working pair for `getline()`. `input` will point to a heap buffer holding the line; `input_size` holds that buffer's **capacity**, not the number of characters read. `size_t` is the right type here: it is the type `sizeof` yields, it is unsigned (sizes are never negative), and it is what `getline` expects.
- `args` is deliberately left uninitialized at declaration, but it is assigned by `split_commands()` before it is ever read, so no indeterminate value is used. Initializing it to `NULL` anyway would be tidier and would make `free_tokens(args)` safe even on a pathological first iteration — a small habit improvement.
- `initial_directory = getcwd(NULL, 0)` records where the shell started. With `buf = NULL, size = 0`, `getcwd` allocates a buffer itself and returns it — the caller must `free()` it later (the same technique as `command_pwd`, Section 6.3). Honest note: `command_cd` currently ignores this parameter (`built_in_commands.c:5` casts it to `(void)`); I keep it because a future `cd -` ("return to the previous directory") feature needs exactly this value. Until then it is an unused-but-intentional parameter.

The prompt is printed with a plain `printf` and no `fflush`. On typical terminal setups this works, but explicitly flushing after the prompt is the portable habit, because output is not guaranteed to appear before the program blocks on input.

### 4.3 Reading input with getline()

```c
if (getline(&input, &input_size, stdin) == -1)
{
    perror("getline");
    break;
}
```

`getline()` is a POSIX 2008 function (declared in `<stdio.h>` on systems that support it) that reads an entire line into a dynamically sized buffer. That is ideal for a shell: I cannot know in advance how long the user's command will be, and `getline` grows the buffer as needed.

| Argument | Type | Why the address is passed |
|---|---|---|
| `lineptr` | `char **` | `getline` may allocate or resize the buffer and must **update the pointer itself**. Passing `input` would change only a copy of the pointer; `&input` lets the callee reach back and modify my variable. |
| `n` | `size_t *` | `getline` reads the current capacity through it and writes the new capacity back after resizing. Same reasoning: the callee must modify my variable, so I pass its address. |
| `stream` | `FILE *` | Where to read from — `stdin`, normally the keyboard. |

The before/after picture for the first call:

```
char *input = NULL;  size_t n = 0;   /* no buffer yet */
getline(&input, &n, stdin);          /* user types: hi */
/* getline allocated a buffer, input now points to "hi\n\0", return value is 3 */
```

The return value is the number of characters read, **including the trailing `'\n'` when present** and excluding the terminating `'\0'`. The return type is `ssize_t`, not `size_t`, precisely because it must be able to represent `-1`: `size_t` is unsigned, so a negative "count" would be impossible to express in it. `getline` returns `-1` when it encounters end-of-file before reading any characters (Ctrl+D at an empty prompt) or when an error occurs. Either way, no more input is available, so the code reports the condition and breaks out of the loop.

Two honest observations about this call site. First, the included `'\n'` is not noise: the tokenizer (Section 5) treats `'\n'` as a delimiter precisely because `getline` leaves it in the buffer. Second, on EOF the C library may not set `errno`, so `perror("getline")` can print a misleading message such as `getline: Success`; distinguishing EOF from a real error with `feof(stdin)` would be more robust (Section 10).

The buffer strategy across the loop is worth stating plainly: I do **not** free `input` after each command. `getline` reuses and grows the same buffer for the lifetime of the shell, so there is exactly one allocation to release, at shutdown: `free(input)` after the loop.

### 4.4 The loop: prompt, guard, dispatch, cleanup

```c
while (1)
{
    printf("[Kinux-Shell]>> ");
    if (getline(...) == -1) { perror("getline"); break; }
    args = split_commands(input);
    if (args && args[0])
        built_in_commands(args, &env, initial_directory);
    free_tokens(args);
}
free(input);
free(initial_directory);
```

- `while (1)` is the read–eval–print loop (REPL): a shell must keep asking for commands until the user leaves, so the loop has no ordinary exit condition — leaving happens through `break` (input exhausted) or `exit()` (the `exit`/`quit` commands).
- `if (args && args[0])` guards against two non-executable outcomes. `args == NULL` happens when `split_commands` rejects the input (too many tokens, `command_parser.c:36–41`). `args[0] == NULL` happens when the line contains nothing but whitespace — pressing Enter produces an empty token array. In both cases there is nothing to run, so the shell simply loops again.
- `free_tokens(args)` runs on **every** iteration, even when `args` is `NULL` (the function tolerates that). This is the discipline that keeps the shell leak-free per command: the token array is temporary by design — allocate, use, release, every iteration.
- On loop exit (EOF), the two long-lived allocations are released explicitly: the `getline` buffer and the initial-directory string. When the user instead types `exit`, the dispatcher calls `exit(EXIT_SUCCESS)` directly (`main.c:52`), which terminates the process immediately and skips those `free()` calls. The operating system reclaims everything at process exit, so this is harmless in practice, but it is worth knowing that `exit` is a shortcut rather than a clean unwind.
- `run_shell` ignores the dispatcher's return value, which means command exit statuses are currently **discarded** — there is no `$?`. Propagating them is a natural future improvement (Sections 7.2 and 10).

### 4.5 built_in_commands(): the dispatcher

```c
int built_in_commands(char **args, char ***env, char *initial_directory)
```

This function answers one question — *is the first token a command I implement myself?* — and either runs the matching built-in or hands the command to the external executor. It is called with `&env` (`main.c:89`), and that address-of is the interesting part.

**Why `char ***env` and not `char **env`?** `command_setenv` builds a completely new environment array and the shell must adopt it. A `char **` parameter would give the dispatcher a *copy of the pointer* to the array — changing that copy would change nothing in `run_shell`. Passing `char ***` (the address of the pointer variable) lets the dispatcher reach back into the caller and replace the pointer itself:

```
run_shell:        env ─────────► [ ptr0, ptr1, ..., NULL ]        (char **)
built_in_commands receives &env  (char ***)
    *env       ──►  the environment array itself                  (char **)
    *env = command_setenv(args, *env);   /* wholesale replacement */
```

Inside the function, `*env` is simply the current array — so `command_echo(args, *env)`, `command_env(*env)`, `command_which(args, *env)`, and `executors(args, *env)` all dereference once to read it, and only `setenv` writes through it. This is the same out-parameter idea as `&num_paths` in Section 7.4, one level deeper.

**Why `strcmp()` and not `==`?** In C, a string *value* has no identity — a "string" is just a char array, and comparing two `char *` with `==` compares **addresses**, not text. `strcmp(args[0], "cd") == 0` walks both strings character by character and returns `0` only when their contents are equal. Two pointers to identical text can easily have different addresses (string literals are not required to be shared), so `args[0] == "cd"` could be false even when the user typed `cd`.

**The dispatch chain** (`main.c:20–58`), in order:

| Match | Action |
|---|---|
| `"cd"` | `command_cd(args, initial_directory)` |
| `"pwd"` | `command_pwd()` |
| `"echo"` | `command_echo(args, *env)` |
| `"env"` | `command_env(*env)` |
| `"setenv"` | `*env = command_setenv(args, *env); return 0` — the only branch that reassigns the environment |
| `".help"` / `"help"` | `display_help()` — the help text lives in `main.c` and is only used here, so it has no header prototype |
| `"which"` | `command_which(args, *env)` |
| `"exit"` / `"quit"` | `exit(EXIT_SUCCESS)` — terminates the whole shell process |
| otherwise | `executors(args, *env)` — **anything unrecognized is treated as an external program** |

The final `else` is the architectural hinge: the dispatcher does not reject unknown commands, it delegates them. One consequence is currently visible as a gap: there is no branch for `unsetenv`, even though the function exists and `which` even lists it as built-in — typing `unsetenv VAR` therefore falls through to the executor and fails (Section 6.7).

Every branch returns an `int` status (`0` on success, nonzero on failure, or the child's exit code from `executors`), but as noted in Section 4.4, the caller currently ignores it.

## 5. command_parser.c — tokenization

### 5.1 Design and scope of the tokenizer

`split_commands()` performs **tokenization**: it takes the input line as one string and produces an array of strings, where each string is one *token* — one individual component of the command. For `ls -l /home` it produces `{ "ls", "-l", "/home", NULL }`.

The tokenizer is whitespace-driven. It splits on five characters (`command_parser.c:20`):

| Delimiter | Why it is in the set |
|---|---|
| `' '` | The obvious separator |
| `'\n'` | `getline` leaves the newline in the buffer, so it must be treated as a delimiter (Section 4.3) |
| `'\r'` | Carriage return, for inputs with Windows-style line endings |
| `'\t'` | Tab |
| `'\a'` | Bell character |

**Scope limitation, stated up front:** this is a simple tokenizer, not a shell parser. It does not understand quotation marks, escaped spaces, pipes, or redirections. `echo "hello   world"` becomes three tokens with collapsed spacing, and a file path containing a space cannot be passed as one argument. Section 10 carries this limitation.

### 5.2 split_commands() step by step

```c
char **split_commands(char *input)
```

**Step 1 — allocate the pointer array.**

```c
size_t buffer_max = MAX_INPUT_SIZE;
char **tokens = malloc(buffer_max * sizeof(char *));
```

`tokens` will hold up to 1024 **pointers**. The `sizeof(char *)` matters: `tokens` is an array of pointers to strings, so I need room for 1024 pointers (typically 8 bytes each on a 64-bit system), not 1024 characters. `malloc` allocates one uninitialized block; it does not allocate any strings — those come later, one `malloc` per token. If `malloc` fails it returns `NULL`; the code reports it with `perror` and exits, because a shell that cannot allocate its parse buffer cannot continue.

**Step 2 — pre-terminate.**

```c
tokens[0] = NULL;   /* safe even if the input turns out to be empty */
```

This guarantees the array is a valid, terminated (empty) token list from the very first moment, so the caller's `args[0]` check can never read uninitialized memory.

**Step 3 — the scan loop.**

```c
for (size_t i = 0; input[i]; i++)
```

`i` indexes into the original input string; the loop runs until the `'\0'`. Three cooperating variables do the work:

- `token` — temporarily points to the **beginning of the current token inside `input`**. It allocates nothing; it stores an address into text I do not own.
- `position` — the index of the next free slot in `tokens`.
- `token_length` — how many characters the current token has; I need it to know how much memory to allocate for that token.

For each iteration:

1. **Skip the separator run** — the inner `while` advances `i` past every whitespace character, so runs of spaces or tabs never produce empty tokens.
2. **Mark the start** — `token = &input[i]`. For `ls -l /home` with `i == 3`, `&input[3]` is the address of `'-'`, so `token` points at the word `-l`. Nothing has been copied yet — `token` is just a lens into the input.
3. **Measure** — the second inner `while` advances `i` to the token's end, counting characters into `token_length`. Now I know *where* the word starts (`token`) and *how long* it is (`token_length`).
4. **Bounds check** —

```c
if (position >= buffer_max - 1)
{
    fprintf(stderr, "Too many command tokens\n");
    free_tokens(tokens);
    return NULL;
}
```

   With 1024 slots, at most ~1023 tokens fit (the last slot is reserved for the `NULL`). When the limit is exceeded, the function reports the problem, releases everything allocated so far, and returns `NULL` — a graceful failure the caller already knows how to handle (Section 4.4).
5. **Allocate the token's own string** —

```c
tokens[position] = malloc((token_length + 1) * sizeof(char));
```

   The `+ 1` reserves one extra byte for the terminating `'\0'` — a string of *n* visible characters needs *n + 1* bytes. `tokens[position]` is a `char *` slot, and this call stores the address of the newly allocated string into that slot. Each token is allocated **separately**: tokens can be any length up to the input's length, and each string is independently owned and freed.
6. **Copy and terminate** — the character-copy loop moves `token_length` bytes from `input`'s text into the new buffer, then writes `'\0'`. For `/home` starting at `input[6]`:

```
tokens[2][0] = token[0];  /* '/' */
tokens[2][1] = token[1];  /* 'h' */
tokens[2][2] = token[2];  /* 'o' */
tokens[2][3] = token[3];  /* 'm' */
tokens[2][4] = token[4];  /* 'e' */
```

   At this point `/home` exists in two places: inside `input` and inside the copy referenced by `tokens[2]`. The copy exists precisely so the token array is independent of the input buffer's lifetime.

7. **Maintain the invariant** — after storing a token, `position++` and `tokens[position] = NULL` re-terminate the array **after every insertion**, so it is never in a state another function could misread. `token_length` resets to 0 for the next token.

**Step 4 — final termination.** After the loop, `tokens[position] = NULL` runs once more. During normal operation this rewrites the same slot step 7 already terminated; its real job is the **empty-input case**, where the loop never ran and the array must still be `{ NULL }`.

The finished structure for `ls -l /home\n`:

```
input (getline's buffer):  "ls -l /home\n\0"
tokens:  [0] ──► "ls"      [1] ──► "-l"      [2] ──► "/home"      [3] ──► NULL
          each string is its own heap allocation, independent of `input`
```

**Why the NULL terminator, and who consumes it?** C arrays do not carry their length, so the array ends with a NULL "sentinel". Consumers in this project: `free_tokens` walks the array until `NULL` to release each string; `run_shell` reads `args[0]` knowing a terminated empty array yields `NULL`; and every array passed to `execve` as `argv` must be NULL-terminated by convention — the kernel's argument-passing depends on it.

### 5.3 Alternative designs, and why I tokenize this way

Two common alternatives are worth naming:

- **A fixed 2D array** (`char tokens[1024][1024]`) — no per-token `malloc`, but ~1 MB of stack memory allocated regardless of need, and every token capped at 1024 characters. Wasteful and limiting.
- **Tokenizing in place** with `strtok()` — no copies at all; tokens would point into the `input` buffer. It would work in this design (the tokens are freed before `getline` refills the buffer), but my version's copies are deliberately independent of the input buffer's lifetime, which keeps the parse result valid no matter when the input is released, and building it by hand taught me the two-level allocation pattern this document keeps returning to.

### 5.4 free_tokens()

```c
void free_tokens(char **tokens)
{
    if (!tokens) return;
    for (size_t i = 0; tokens[i]; i++)
        free(tokens[i]);
    free(tokens);
}
```

`free_tokens` mirrors the two allocation sites of `split_commands`: one `malloc` for the pointer array, one `malloc` per string. The loop releases every individual string, and the final `free(tokens)` releases the array that holds their addresses.

The order is not a style choice: the strings' addresses live **inside** the array, so freeing the array first would orphan every string before it could be reached — the definition of a memory leak. `free(NULL)` is a defined no-op, which is why the guard is technically redundant but still good documentation of intent.

Every allocation in this file is released: `split_commands` itself calls `free_tokens` on its bounds-check error path, and `run_shell` calls `free_tokens` after every command. Nothing in the tokenizer leaks.

## 6. built_in_commands.c — built-in commands

### 6.1 Why some commands must be built-in

A *built-in* command is one the shell executes inside its own process, rather than launching a separate program. For `cd` this is not a style choice — it is the only way the command can work.

The current working directory is a per-process property, and `chdir()` changes it only for the **calling** process. Suppose the shell forked a child just to run `cd /tmp`: the child would change *its own* directory, then terminate — and the parent shell would still be sitting in the original directory. The change would evaporate with the child. For the same reason, `setenv` must modify the shell's own memory (Section 2.3): an environment change confined to a child process dies with it.

`pwd`, `echo`, `env`, and `which` do not strictly require built-in status, but implementing them inside the shell keeps them consistent with the shell's private environment: `echo $VAR` reads the shell's own array (Section 6.4), and `which` can recognize the shell's own built-ins (Section 6.6).

### 6.2 command_cd()

```c
int command_cd(char **args, char *initial_path)
```

`chdir()` — declared in `<unistd.h>`, prototype `int chdir(const char *path)` — attempts to make `path` the calling process's working directory. It returns `0` on success and `-1` on failure (with `errno` describing why).

```c
if (args[1] == NULL)        printf("expected path after cd\n");
else if (chdir(args[1]) == 0) printf("The directory has been changed to %s\n", args[1]);
else                          perror("cd");
```

- No argument: a reminder is printed. Note that there is no fallback to `$HOME` — real shells jump home on bare `cd`; mine does not (Section 10).
- Success: a confirmation message is printed. (An earlier draft of this code left success silent; the current version reports it.)
- Failure: `perror("cd")` prints `cd:` followed by the system's reason — for example `cd: No such file or directory` when the path does not exist.

One honest gap: the function returns `0` even when `chdir` fails. Returning `1` on failure would give the caller (and, one day, `$?`) an accurate status — noted in Section 10.

The `initial_path` parameter is currently unused — `built_in_commands.c:5` casts it to `(void)` to silence the unused-parameter warning. It is kept intentionally for a future `cd -` that returns to the previous directory; the shell already captures the starting directory for exactly that purpose (Section 4.2).

### 6.3 command_pwd()

```c
int command_pwd(void)
{
    char *cwd = NULL;
    cwd = getcwd(NULL, 0);
    if (cwd != NULL) { printf("%s\n", cwd); free(cwd); }
    else perror("getcwd");
    return 0;
}
```

`getcwd()` has two usage forms. The classic form hands it a caller-owned buffer:

```c
char buffer[1024];
getcwd(buffer, sizeof(buffer));   /* path written into my array */
```

The form I use everywhere in this project, `getcwd(NULL, 0)`, asks the implementation to **allocate** a buffer of exactly the right size and return it. That removes the guessing game about how long a path can be — but the memory is now mine, and I must release it with `free()`. The code does so immediately after printing.

That `free()` is the textbook illustration of a **memory leak** avoided: `pwd` can be called a thousand times in a session, and each call allocates a fresh buffer. Without the `free`, every call would strand a buffer that is no longer reachable — allocated memory with no owner, wasted until the process exits. On failure, `getcwd` returns `NULL`, `errno` explains why, and `perror("getcwd")` reports it.

### 6.4 command_echo()

```c
int command_echo(char **args, char **env)
```

`echo` prints its arguments, supports `-n` (suppress the trailing newline), and expands `$VAR` against the shell's environment — which is why it receives both the tokens and the environment array.

**The `-n` flag.** `new_line` starts at `1` (newline enabled — `echo`'s default behavior is to end its output with a newline so the next prompt starts on a fresh line). If the *first* argument is exactly `-n`, the flag is disabled and the argument index skips it, so `-n` is not printed as ordinary text. Only the first argument position is recognized — `echo hello -n` prints `-n` as text.

**The `$VAR` expansion — why not `getenv()`?** For each argument starting with `$`, the code searches the shell's environment array by hand (`built_in_commands.c:49–57`):

```c
if (strncmp(env[j], args[i] + 1, strlen(args[i] + 1)) == 0 &&
    env[j][strlen(args[i] + 1)] == '=')
{
    env_var = env[j] + strlen(args[i] + 1) + 1;
    break;
}
```

Two details carry the correctness:

- **Prefix match plus `'='` boundary.** The `strncmp` compares the first *n* characters, where *n* is the variable name's length — but a prefix alone is not enough: `COLOR` is also the prefix of `COLOR_EXTRA=red`. The second condition requires the character right after the name to be `'='`, which is the structural boundary of an environment entry. `echo $COLOR` therefore matches only a variable actually named `COLOR`.
- **Pointer arithmetic to skip the name.** `args[i] + 1` steps past the `$`; `env_var = env[j] + strlen(name) + 1` points at the first character after the `'='` — the value. Nothing is copied; `env_var` simply points into the environment array.

And the design decision: this scan exists because of Section 2.3. `getenv()` reads the *process* environment (`environ`), which `setenv` never touches — after the first `setenv`, the two views diverge, and `getenv("COLOR")` would miss a variable I set a moment ago. Scanning the shell's own array means `echo` always reports the shell's authoritative view, including variables created in this session. If the variable is not found, the function prints `Not Found ` (note: with a trailing space — a cosmetic detail).

**Spaces and newlines.** The intended behavior: a single space between arguments, a newline after the last argument (unless `-n`). The mechanics (`built_in_commands.c:65–73`) are precise and worth walking through for `echo Hello world`:

- Iteration 1 (`args[1] == "Hello"`): print `Hello`. The next argument exists, so print one space and set `new_line = 0`; the newline check therefore does nothing. Then — and this is the subtle part — `new_line` is **reset to 1** at the end of every iteration.
- Iteration 2 (`args[2] == "world"`): print `world`. No next argument, so no space; `new_line` is still `1`, so the `'\n'` prints. Output: `Hello world\n`.

Because of that per-iteration reset, two quirks follow from the code as written, and I document them rather than gloss over them:

- Bare `echo` prints **nothing** — not even a newline — because the loop never runs when `args[1]` is `NULL` (a real `echo` prints just the newline).
- `echo -n Hello world` still ends with a newline: iteration 1's reset restores `new_line = 1`, so the last argument prints the `'\n'` despite `-n`. The `-n` flag only survives when it is the *only* content decision in a single-argument echo.

Both behaviors are faithful to the source (`built_in_commands.c:73`) and are listed as quirks in Section 10 with the obvious fix (reset `new_line` only when it was not disabled by `-n`, and print a newline when no arguments follow).

### 6.5 command_env()

```c
int command_env(char **env)
{
    size_t i = 0;
    while (env[i] != NULL)
    {
        printf("%s\n", env[i]);
        i++;
    }
    return 0;
}
```

Each environment variable is one string `"NAME=value"`, and the array ends with `NULL` — so a plain walk-until-NULL prints the whole environment:

```
env[0] ──► "HOME=/home/user"
env[1] ──► "PATH=/usr/bin:/bin"
env[2] ──► "USER=user"
env[3] ──► NULL
```

This small function is also the reference example of *why* environment arrays are NULL-terminated: the loop needs no count, only the sentinel. Every other consumer of `env` in the project (`echo`'s scan, `get_path`, `count_env_vars`, `execve`'s `envp`) relies on the same convention.

### 6.6 command_which() and find_command_in_path()

#### 6.6.1 command_which()

`which` answers the question *where does this command live?* — for built-ins it reports membership; for external programs it prints the full executable path.

With no argument it prints a usage reminder and returns `1`. The first check is against a table of the shell's own built-ins:

```c
const char *built_in_commands[] = {"cd","pwd","echo","env","setenv","unsetenv","which","exit","quit",NULL};
```

Each element is a pointer to **constant** characters: `const` documents that these strings are read-only lookup data — an assignment like `built_in_commands[0][0] = 'c'` would be a compile error. Since the table is only ever read and compared, `const` is both accurate and self-documenting.

If the name is not in the table, the search moves to the filesystem: `full_path = find_command_in_path(args[1], env)`. On success the path is printed and — importantly — **freed by the caller**, because `find_command_in_path` allocates the string it returns (Section 6.6.2 explains why). On failure, `which: <name>: command not found` is printed and the function returns `1`.

Two honesty notes about the table as written: it lists `unsetenv`, which the dispatcher never actually dispatches (Section 6.7), and it omits `.help`/`help`, so `which help` reports "command not found". Cosmetic gaps, listed in Section 10.

#### 6.6.2 find_command_in_path() — the core of `which`

```c
char *find_command_in_path(const char *command, char **env)
```

The return type is `char *`, not an `int`, because the caller wants the path itself, not merely a yes/no: given `which gcc`, this function returns a heap-allocated `"/usr/bin/gcc"` or `NULL`.

**Step 1 — find `PATH`.** The environment is an unordered array of `"NAME=value"` strings, so the code walks it and tests only a *prefix*:

```c
if (strncmp(env[i], "PATH=", 5) == 0) { path_env = env[i] + 5; break; }
```

`strncmp` compares just the first five characters — a full `strcmp` would always fail, because every entry continues with a value after the name. `path_env = env[i] + 5` steps past `"PATH="` to point at the value itself. If the loop finishes without a match, the function returns `NULL` and the caller reports "command not found".

**Step 2 — duplicate `PATH` before tokenizing.**

```c
path_copy = strdup(path_env);
```

`strdup()` allocates `strlen(s) + 1` bytes, copies the string including `'\0'`, and returns the copy (a POSIX function from `<string.h>`). The copy is required because the next step, `strtok()`, **modifies the string it tokenizes** — it overwrites each `':'` delimiter with `'\0'` as it walks. Tokenizing in place would corrupt the shell's environment array; duplicating first means the damage lands on memory I own. This is the general rule: `strdup` whenever a function needs a modifiable, separately-owned copy of a string that belongs to someone else.

**Step 3 — split the copy into directories.** `PATH` looks like `/usr/local/bin:/usr/bin:/bin`; `strtok(path_copy, ":")` returns the first directory and subsequent `strtok(NULL, ":")` calls walk the rest. Only the copy is shredded; the original `PATH` entry in the environment is untouched.

**Step 4 — build the candidate path.** For each directory:

```c
size_t len = strlen(token);
if (token[len - 1] != '/') snprintf(full_path, sizeof(full_path), "%s/%s", token, command);
else                       snprintf(full_path, sizeof(full_path), "%s%s",  token, command);
```

The length check handles `PATH` entries written with a trailing slash (`/bin/`) by avoiding a doubled slash. `snprintf` is `printf` writing into a buffer instead of the terminal, with one crucial extra argument — the destination size — which makes it truncate safely instead of overflowing. `sprintf` has no such bound and must only be used when the buffer size is provably sufficient; the project uses `snprintf` for every path construction for exactly that reason.

`full_path` itself is a 1024-byte stack array, so an over-long candidate is truncated by `snprintf` rather than corrupting memory.

**Step 5 — ask the OS whether it is executable.**

```c
if (access(full_path, X_OK) == 0)
{
    free(path_copy);
    return strdup(full_path);   /* heap copy that outlives this function */
}
```

`access()` (POSIX, `<unistd.h>`, prototype `int access(const char *pathname, int mode)`) checks whether the calling process could access the file in the requested way; `X_OK` asks specifically about **execute** permission — `which` is hunting executables, not readable text files. `0` means the check passed.

The final `return strdup(full_path)` is a memory-lifetime lesson worth pausing on. `full_path` is an **automatic local variable**: it lives on the stack frame of this function call, and that frame dies at `return`. `return full_path;` would hand back a pointer into memory that no longer exists — a dangling pointer, and using it is undefined behavior. `strdup` makes a heap copy whose lifetime continues until someone calls `free()` on it. That is why the caller, `command_which`, later does `free(full_path)` — allocation and deallocation responsibilities stay symmetric across the call boundary.

`path_copy` is freed on **both** exits — the success return above and the fall-through return at the end of the loop — so the tokenizer's scratch memory never leaks.

### 6.7 Modifying the environment: count_env_vars(), command_setenv(), command_unsetenv()

Both commands follow the same pattern: build a **new** environment array and return it, so the dispatcher can swap it in with `*env = command_...(args, *env)`. Returning `char **` is what makes the wholesale replacement of Section 4.5 possible.

**Current wiring status, stated honestly:** `setenv` is dispatched (`main.c:38`), but the dispatcher has **no branch for `unsetenv`** — even though the function below is fully implemented, and both `.help` and `which` advertise it. Typing `unsetenv VAR` therefore falls through to the external executor and fails. Wiring it in would be a three-line branch; it is the top item in Section 10.

#### 6.7.1 count_env_vars()

```c
int count_env_vars(char **env)
{
    size_t count = 0;
    while (env[count] != NULL) count++;
    return (int)count;
}
```

A small helper used by both commands: walk until the `NULL` sentinel and count. The count is the basis for every allocation size that follows.

#### 6.7.2 command_setenv()

**Two input formats.** The tokenizer knows nothing about `=`, so the command accepts both spellings:

- `setenv COLOR=blue` → `args[1] = "COLOR=blue"`, `args[2] = NULL`. The code simply does `new_var = strdup(args[1])` — the string already has the right shape.
- `setenv COLOR blue` → `args[1] = "COLOR"`, `args[2] = "blue"`. Here the code builds the entry itself:

```c
new_var = malloc(strlen(args[1]) + strlen(args[2]) + 2);
if (new_var) sprintf(new_var, "%s=%s", args[1], args[2]);
```

The `+ 2` accounts for the `'='` and the `'\0'` — the buffer is sized **exactly**, which is what makes `sprintf` acceptable in this one place. It is still the only `sprintf` in the project; everywhere else I use `snprintf` (Section 6.6.2), and converting this call would be the more defensive habit.

**Sizing the new array.**

```c
char **new_env = malloc((env_count + 2) * sizeof(char *));
```

The formula is `existing + new + NULL`: with three existing variables, that is five pointer slots. `sizeof(char *)` — the same lesson as Section 5.2 — allocates room for *pointers*; the strings themselves are allocated one `strdup` at a time in the copy loop.

**Copying the existing entries, with rollback on failure.**

```c
for (int i = 0; i < env_count; i++)
{
    new_env[i] = strdup(env[i]);
    if (!new_env[i])
    {
        for (int j = 0; j < i; j++) free(new_env[j]);
        free(new_env);
        return env;   /* original environment, untouched */
    }
}
```

Why copy at all? Because the new array must contain *everything the old one had* plus the new variable. Why the per-copy check? `strdup` can fail, and a half-built environment must not be returned. The cleanup walks back over the `i` copies already made, releases them, releases the array, and returns the original environment unchanged. Note what is **not** freed: `env[i]` — those strings belong to the original array, not to the copies; freeing them would destroy live data.

**Appending and terminating.**

```c
new_env[env_count]     = new_var;
new_env[env_count + 1] = NULL;
```

The new variable takes the first free slot and `NULL` closes the array — the sentinel every consumer depends on (Section 6.5).

**What the function does *not* do — two documented gaps.**

1. **It appends; it never replaces.** Running `setenv PATH=...` twice produces two `PATH` entries; real `setenv` implementations update the existing entry in place. A replace-first-then-append pass would be the fix (Section 10).
2. **The old array is abandoned.** On success the dispatcher repoints `env` at the new array, and the previous array — plus any heap strings in it — is simply left behind. Why not free it? Because its contents may be *system-owned*: before the first `setenv`, the array *is* the process environment (`environ`), and freeing strings the C library owns is undefined behavior. The array mixes system-owned and heap-owned strings across generations, and without extra bookkeeping there is no safe way to separate them. The result is a bounded leak — one environment-sized allocation per successful `setenv` — which I accept in this version and document as a design trade-off (full discussion in Section 9). Production shells solve this by tracking which strings they own.

#### 6.7.3 command_unsetenv()

```c
char **command_unsetenv(char **args, char **env)
```

The mirror image of `setenv`: remove one variable and return the rebuilt array. (Remember: currently unreachable from the dispatcher — Section 6.7's introduction.)

**Sizing.**

```c
char **new_env = malloc((env_count + 1) * sizeof(char *));
```

One slot less than `setenv` needs, because nothing is added: the worst case is that the variable is not found and **all** `env_count` entries are retained, plus the `NULL` terminator.

**Two indexes with different jobs.** The loop walks the old array with `i` and fills the new one with `j`. They advance at different rates, because the entry being removed is skipped:

```
env[0] = "HOME=/home/user"     new_env[0] = "HOME=/home/user"
env[1] = "PATH=/usr/bin:/bin"  new_env[1] = "PATH=/usr/bin:/bin"
env[2] = "COLOR=blue"          (skipped — i advances, j does not)
env[3] = "USER=user"           new_env[2] = "USER=user"
                               new_env[3] = NULL
```

**The match test** is the same two-part pattern as `echo`'s `$VAR` scan (Section 6.4): a `strncmp` prefix comparison on the variable name, **plus** the `'='` boundary check. The prefix alone would falsely match `unsetenv COLOR` against `COLOR_EXTRA=red`; requiring `env[i][strlen(name)] == '='` pins the match to an exact variable name. This is a general lesson: string-prefix comparisons usually need a boundary check to be meaningful.

**On a match:** `found = 1` and the entry is skipped — nothing else. The current code does **not** free `env[i]`, for the same ownership reason as in `setenv`: the string may belong to the system's `environ`, and freeing it could be undefined behavior. The whole old array is abandoned on success, exactly as in `setenv` (Section 9 covers the accounting).

**On a non-match:** the entry is retained as an *independent copy* — `new_env[j] = strdup(env[i])` — with the same rollback discipline as `setenv`: if a `strdup` fails, the copies made so far (`new_env[0..j-1]`) are freed, the array is freed, and the original environment is returned untouched.

**If the variable was never found:** `found` is still `0`, the function reports `Variable <name> not found in environment`, releases the `j` copies it did make and the array itself, and returns the original environment. Cleaning up a partially built result instead of returning it half-populated is the consistent rule in both commands.

**Terminating:** `new_env[j] = NULL` closes the array. Without the sentinel, every later consumer — `echo`'s scan, `count_env_vars`, `execve` — would read past the end. The example above shows the finished array with its `NULL`.

## 7. external_commands.c — process creation and execution

Everything so far runs *inside* the shell process. This file handles the other kind of command: `ls`, `gcc main.c -o main`, `./program` — separate executable programs that live on disk. The shell cannot become those programs (it must survive to show the next prompt), so it creates a new process and lets that process become the command. Four concepts build on each other:

1. `fork()` — creates a child process by duplicating the shell.
2. `execve()` — replaces the program running in a process with the requested executable.
3. `waitpid()` — lets the parent wait for the child and collect its status.
4. `PATH` — tells the shell where to look for the executable.

The distinction to hold onto: `fork()` creates a process; `execve()` does **not** create anything — it swaps the program inside an existing process. Together they are the standard Unix recipe: *fork, then the child execs, while the parent waits*.

### 7.1 executors()

```c
int executors(char **args, char **env)
```

(The name ends in `s` — it executes external commands.)

A defensive guard comes first: `if (!args || !args[0]) return 0;` — nothing to execute, nothing to fork.

**Creating the child.**

```c
pid = fork();
```

`fork()` asks the OS to duplicate the calling process. Its famous property: it is **called once but returns twice** — once in each process — with different values telling you which one you are:

| Return | Meaning |
|---|---|
| `-1` | Creation failed (reported with `perror`, `executors` returns `1`) |
| `0` | You are the **child** — the fresh copy |
| positive `pid` | You are the **parent** — the number is the child's PID |

`pid_t` is the POSIX type for process IDs — a number the OS assigns to identify a running process. I use `pid_t` rather than assuming it is a plain `int`, because the type abstracts the platform detail.

**The child's side.**

```c
if (pid == 0)
{
    if (child_process(args, env))
        _exit(EXIT_FAILURE);
    _exit(EXIT_SUCCESS);
}
```

The child calls `child_process()` (Section 7.2), which either replaces the process image with the requested program — in which case nothing after it ever runs — or fails and returns nonzero, in which case the child terminates itself with `_exit()`.

**Why `_exit()` and not `exit()`?** Both terminate the process, but `exit()` first flushes stdio buffers and runs handlers registered with `atexit`. In a forked child that is dangerous: the child *inherited copies of the parent's stdio buffers*, so an `exit()` in the child could flush the parent's pending output a second time — duplicated prompt text or log lines. `_exit()` terminates immediately, with no stdio flushing — the correct tool in a child process that never set up stdio of its own.

One honest observation: `child_process()` returns `1` on **every** failure path and never returns at all after a successful `execve` (the process image is replaced), so the trailing `_exit(EXIT_SUCCESS)` is effectively unreachable. Every return from `child_process` means "execution failed". Also worth noting: any memory still allocated in the child at this point is irrelevant — the process is about to disappear and the OS reclaims everything — yet the code frees its PATH data anyway (Section 7.2), which is the better habit.

**The parent's side — waitpid().**

```c
if (waitpid(pid, &status, 0) == -1) { perror("waitpid"); return 1; }
```

The parent must wait: otherwise it would print the next prompt while `ls` is still writing to the terminal, and output from the shell and the child would interleave unpredictably. For a foreground shell, waiting is what makes one command finish before the next begins.

The three arguments: `pid` selects which child to wait for; `&status` is the address of my integer where the OS writes the termination info (an address, again, because the callee must modify my variable — the same pattern as `getline` and `&num_paths`); and `0` means *block until this child changes state* — the option `WNOHANG` would instead return immediately, which background-job implementations use. On success `waitpid` returns the child's PID; on failure `-1` (it can be interrupted by a signal).

**Decoding the status.** `status` is an *encoded* value — a bitfield, not the raw exit code — and `<sys/wait.h>` provides the macros to decode it:

```c
if (WIFSIGNALED(status))
{
    printf("Process terminated by signal: %d\n", WTERMSIG(status));
    return 128 + WTERMSIG(status);
}
if (WIFEXITED(status))
    return WEXITSTATUS(status);
```

- `WIFSIGNALED(status)` — did the child die from a signal (say, a program that crashed with `SIGSEGV`)? `WTERMSIG(status)` yields *which* signal. The return value `128 + signal` is the shell-world convention for signal deaths — bash returns `130` for Ctrl+C (`SIGINT` is 2) and `139` for a segfault (11) — so my shell speaks the same dialect.
- `WIFEXITED(status)` / `WEXITSTATUS(status)` — the child terminated normally; this extracts the exit code the child chose.

`executors` therefore returns the external command's exit status — but recall from Section 4.4 that `run_shell` currently discards it. The plumbing is ready for `$?`; only the consumer is missing (Section 10).

### 7.2 child_process() — locate and launch

This function runs **only in the child**. Its job: turn `args[0]` — a bare name like `ls`, or a path like `./program` — into an executable file, and become it.

**Step 0 — the `'/'` shortcut.**

```c
if (strchr(args[0], '/') != NULL)
{
    execve(args[0], args, env);
    perror("execve");
    return 1;
}
```

If the command contains a slash, the user has already specified *where* the program is (`./program`, `/bin/ls`, `../tools/build`), so there is nothing to search: `execve` is called directly on the name as given. `strchr` returns a pointer to the first `'/'` in the string, or `NULL`. This is the branch that makes `./program` from the lifecycle diagram work — and it is also the branch that turns a typo like `./progrm` into `execve: No such file or directory`.

**Step 1 — fetch `PATH`.** `path_string = get_path(env)` (Section 7.3) returns a heap copy of the `PATH` value, or `NULL` if there is no `PATH`. A missing `PATH` is not fatal — it just skips straight to the current-directory fallback.

**Step 2 — split `PATH` into directories.**

```c
path_list = split_paths(path_string, &num_paths);
```

`&num_paths`, not `num_paths`: `split_paths` must *report back* how many directories it found, which means it must modify the caller's variable — the out-parameter pattern again (fully explained in Section 7.4).

**Step 3 — the search loop.**

```c
for (int i = 0; i < num_paths; i++)
{
    char full_path[MAX_INPUT_SIZE];
    int path_length = snprintf(full_path, sizeof(full_path), "%s/%s", path_list[i], args[0]);
    if (path_length < 0 || path_length >= (int)sizeof(full_path))
        continue;   /* candidate too long — skip it safely */
    if (access(full_path, X_OK) == 0)
        execve(full_path, args, env);
}
```

For each directory, the candidate is `directory + "/" + command`: `path_list[i] = "/usr/bin"`, `args[0] = "ls"` → `"/usr/bin/ls"`. `snprintf` bounds the write (Section 6.6.2), and its **return value** is checked as well: it reports the length the output *would* have had, so a value `>= sizeof(full_path)` means truncation occurred — this version skips the candidate rather than testing a truncated path (slightly more rigorous than the `which` search in Section 6.6, which truncates silently — one of the inconsistencies noted in Section 10).

`access(full_path, X_OK)` then asks the OS: *can this process execute this file?* If yes, `execve(full_path, args, env)` is called — and on success it **never returns**: the child stops being the shell and *becomes* `ls`. If `execve` fails despite the `access` pass (the file changed between the two calls, or it is not a valid executable format), the loop simply continues to the next directory.

The three `execve` arguments, once and for all:

| Argument | Meaning |
|---|---|
| `pathname` | Full path of the executable to load |
| `args` | The argument array — `args[0]` is conventionally the command name, the rest are its arguments, terminated by `NULL` |
| `env` | The environment array handed to the new program — my shell's private view, per Section 2.3 |

**Step 4 — release the PATH data.** After the loop, the per-directory strings, the array, and `path_string` are all freed. Nothing further needs them, and the coming fallback is independent — and freeing here (rather than only at process death) keeps the function correct for any future caller.

**Step 5 — the current-directory fallback.** If nothing in `PATH` launched, the code tries the working directory:

```c
cwd = getcwd(NULL, 0);                                        /* heap — freed below */
int path_length = snprintf(full_cwd_path, sizeof(full_cwd_path), "%s/%s", cwd, args[0]);
if (path_length < 0 || path_length >= (int)sizeof(full_cwd_path))
    { fprintf(stderr, "Command path is too long\n"); free(cwd); return 1; }
execve(full_cwd_path, args, env);
perror("execve");
free(cwd);
return 1;
```

Same construction pattern, same length check, same `X_OK`-free direct attempt. One behavioral honesty note: real shells do **not** search the current directory unless it is listed in `PATH` (a deliberate security choice), so my fallback makes `ls` work from the current directory in cases where bash would require `./ls`. It is a documented divergence, not an accident — and it is what guarantees every failure ends with a clear `execve: ...` error instead of silence.

### 7.3 get_path()

```c
char *get_path(char **env)
```

Walks the environment array testing the `"PATH="` prefix with `strncmp(env[i], "PATH=", 5)` — the identical technique to `find_command_in_path` (Section 6.6.2), so I will not repeat the reasoning; note that the project has two parallel `PATH` searchers (Section 10 proposes consolidating them).

The value is returned as `strdup(env[i] + 5)` — a heap copy, for the same `strtok`-destroys-what-it-tokenizes reason as in Section 6.6.2: the caller will feed this string to `split_paths`, which modifies what it splits. The ownership is symmetric: `get_path` allocates, `child_process` frees (`external_commands.c:100`). On failure (no `PATH`, or allocation failure) it returns `NULL`, and the caller's `path_string != NULL` check routes around it.

### 7.4 split_paths()

```c
char **split_paths(char *paths, int *count)
```

The purpose: turn `"/usr/local/bin:/usr/bin:/bin"` into `{"/usr/local/bin", "/usr/bin", "/bin", NULL}` and report the count. It demonstrates two techniques at once: growing an array with `realloc`, and returning two results through different channels — the array via the **return value**, the count via the **pointer parameter**. A C function can return only one value; when a function must hand back an array *and* its length, one of them has to travel as an out-parameter.

**Setup.** After `*count = 0` and the `NULL`-input guard, the function duplicates its input:

```c
paths_copy = strdup(paths);
```

`strtok` will overwrite the delimiters of whatever it tokenizes, so a private copy is mandatory — even though the caller's `path_string` is already a copy, duplicating again keeps this function self-contained and safe with any input.

**The growth loop.**

```c
token = strtok(paths_copy, ":");
while (token)
{
    char **temp = realloc(result, ((*count + 2) * sizeof(char *)));
    if (!temp) { /* free everything; *count = 0; return NULL */ }
    result = temp;

    result[*count] = strdup(token);   /* own copy of this directory */
    if (!result[*count]) { /* same rollback */ }

    (*count)++;
    result[*count] = NULL;            /* array stays terminated */
    token = strtok(NULL, ":");
}
free(paths_copy);
```

Four things here repay close reading:

- **The `realloc` temp-pointer pattern.** `realloc(ptr, n)` resizes a block and may *move* it, returning a new address — or `NULL` on failure, **leaving the original block untouched**. Writing `result = realloc(result, ...)` directly would, on failure, overwrite the only pointer to the still-valid original block: a leak *and* a dangling pointer. Assigning to `temp` first, checking, and only then committing preserves the original on failure. This is one of the classic C memory-management idioms.
- **`realloc(NULL, n)` behaves like `malloc(n)`.** `result` starts as `NULL`, so the first iteration allocates from scratch — no special case needed.
- **The `+ 2` sizing.** Existing `*count` entries, plus one for the new directory, plus one for the `NULL` terminator. The array is re-terminated after every append (`result[*count] = NULL`), so it is never half-valid.
- **`strdup(token)`, not a pointer into `paths_copy`.** The tokens point into memory that is about to be freed (`free(paths_copy)` at the end). Copying each directory gives the result array independent ownership — the same lifetime principle as returning `strdup(full_path)` in Section 6.6.2.

**Failure semantics.** Every failure path frees everything built so far, resets `*count` to `0`, and returns `NULL`. In the caller this degrades gracefully: an empty directory list means the search loop simply never runs, and the current-directory fallback still gets its chance.

### 7.5 Alternatives and caveats

- **`execvp()` does this for you.** `execvp(args[0], args)` searches `PATH` internally and would replace `get_path` + `split_paths` + the loop entirely (at the cost of less control and a different environment mechanism). I built the search by hand deliberately — understanding *how* a shell finds executables is one of the main learning goals of this project, and the manual version keeps the environment handling fully explicit.
- **The `access()`-then-`execve()` sequence has a race** (time-of-check-to-time-of-use): the file could change between the permission check and the execution. Production shells skip the check and call `execve` directly, using `errno` (`ENOENT` → keep searching) to decide. Mine is simpler and adequate for a foreground learning shell; noted for completeness.
- **Two parallel `PATH` searchers.** `find_command_in_path` (`which`) and `child_process` + `get_path` + `split_paths` (the executor) solve the same problem with slightly different rigor (silent truncation vs. skip-on-truncation). Consolidating into one shared helper is the top structural improvement in Section 10.

## 8. The Makefile

### 8.1 Why make

Without a Makefile, building this project by hand means typing the full command every time:

```
gcc main.c command_parser.c built_in_commands.c external_commands.c -o Kinux
```

(Omitting any of the four `.c` files produces link errors — the older version of this document listed only three.) As the project grows, that command only gets longer. A Makefile records the build instructions once, and `make` reads them and runs what is needed.

There is a second benefit the hand-rolled command cannot match: **incremental builds**. `make` compares timestamps between sources and outputs, and recompiles only what changed. Editing `command_parser.c` rebuilds one object file and relinks; untouched files are not recompiled.

### 8.2 Variables

```make
TARGET = Kinux
SRC    = main.c command_parser.c built_in_commands.c external_commands.c
OBJ    = $(SRC:.c=.o)
CC     = gcc
CFLAGS = -Wall -Wextra -Werror
```

`TARGET`, `SRC`, `OBJ`, `CC`, and `CFLAGS` are Makefile variables; the names are conventions, not requirements, but conventions make a Makefile readable to other developers. The interesting line is `OBJ = $(SRC:.c=.o)` — a **substitution reference** that replaces every `.c` suffix with `.o`, producing `main.o command_parser.o built_in_commands.o external_commands.o`. Adding a source file means editing `SRC` only; `OBJ` and the rules follow automatically.

### 8.3 Targets, rules, and .PHONY

```make
all: $(TARGET)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@
```

A **target** names something `make` can build or do; its prerequisites are what it depends on; the tab-indented lines are the recipe. `all` is the **default target** — the first one listed — so bare `make` builds it: it needs `Kinux`, which needs the four object files, which the pattern rule below produces.

- The **pattern rule** `%.o: %.c` is a template: for any stem X, `X.o` is built from `X.c`. That single rule covers all four object files.
- Inside a rule, `$<` expands to the **first prerequisite** (the `.c` file) and `$@` to the **target** (the `.o` file). So for `main.o`, the recipe becomes `gcc -Wall -Wextra -Werror -c main.c -o main.o`.
- `-c` means *compile only, do not link* — it produces an object file. The linking step happens once, in the `all` rule, where the four `.o` files are combined into the executable.

```make
clean:
	rm -f *.o
fclean: clean
	rm -f $(TARGET)
re: fclean all
.PHONY: all clean fclean re
```

- `clean` removes the object files (`rm -f *.o`: `-f` forces removal without confirmation and stays silent when nothing matches). They are intermediate files — regenerated from sources at any time.
- `fclean` ("full clean") removes the executable too — useful before a from-scratch rebuild, so no stale binary can be run by mistake.
- `re` declares both `fclean` and `all` as prerequisites, so `make re` tears everything down and rebuilds.
- `.PHONY` declares that these targets are *actions, not files*. Without it, a stray file called `clean` in the directory would make `make clean` say "nothing to be done" (the file exists and is up to date). Phony targets are always considered out of date.

### 8.4 Compiler flags

| Flag | Effect |
|---|---|
| `-Wall` | Enables most of the warnings that catch real bugs |
| `-Wextra` | Adds further warnings, including unused function parameters — which is why `main.c` needs the `(void)argc;` casts of Section 4.1 |
| `-Werror` | Turns every warning into a build error |

`-Werror` is the strictest of the three: the code must compile not just correctly, but *warning-free*. For a learning project this is a deliberate discipline — the build refuses to proceed past anything the compiler considers suspicious.

### 8.5 What `make` does, end to end

Running `make` on a clean tree: `all` → needs `Kinux` → needs the four `.o` files → the pattern rule compiles each `.c` into its `.o` → the `all` recipe links them into `Kinux`. Running `make` again does nothing ("up to date"); editing one source file recompiles exactly that object file and relinks.

## 9. Memory-management summary

Every heap allocation in the project, with its owner and its release point. Reading this table after Sections 4–7 is the fastest way to see that the accounting closes:

| Allocation | Allocated by | Released by / when |
|---|---|---|
| `getline` buffer (`input`) | `getline`, on first use | `run_shell`, once at shutdown — reused across all iterations |
| Token array + each token string | `split_commands` | `free_tokens`, every iteration (and on its own bounds-check failure path) |
| Initial directory (`initial_directory`) | `getcwd(NULL, 0)` in `run_shell` | `free(initial_directory)` at shutdown — currently unused otherwise |
| `cwd` in `command_pwd` | `getcwd(NULL, 0)` | Immediately after printing |
| `cwd` in `child_process` | `getcwd(NULL, 0)` | After the current-directory attempt |
| `path_copy` in `find_command_in_path` | `strdup` | Both exits of the function |
| `full_path` returned by `find_command_in_path` | `strdup` inside the callee | The caller (`command_which`) after printing |
| `path_string` from `get_path` | `strdup` in the callee | `child_process`, after the `PATH` loop |
| `path_list` + each directory string | `split_paths` (`strdup` per entry) | `child_process`, after the `PATH` loop |
| New environment arrays (`setenv`/`unsetenv`) + their duplicated strings | `malloc`/`strdup` | **Never — deliberately abandoned** (see below) |

The rules this project follows, in priority order:

1. Every `malloc`/`strdup`/`realloc` result is freed exactly once, by the function that took ownership of it.
2. Free only what you allocated — never what you borrowed. The original environment strings belong to the C library (`environ`); freeing them would be undefined behavior (Section 2.3).
3. Release in dependency order: the strings before the array that holds their addresses (Section 5.4).
4. Failure paths roll back partial work: `setenv`, `unsetenv`, and `split_paths` all free everything they allocated before bailing out.
5. A process about to die or be replaced does not strictly need to free anything — the OS reclaims it all — but this code frees anyway, as a matter of habit and future-proofing.
6. `realloc` goes through a temp pointer, so a failed resize cannot orphan the original block (Section 7.4).
7. Never return a pointer to automatic (stack) memory; return a `strdup`'d copy instead (Section 6.6.2).

**The one known leak, stated plainly.** Each successful `setenv`/`unsetenv` abandons the previous environment array wholesale, because its contents mix system-owned strings (from `environ`) with heap-owned copies from earlier rebuilds, and there is no safe way to separate them without additional bookkeeping. The leak is bounded — one environment-sized allocation per successful call — and fixing it properly means tagging string ownership (production shells do exactly that). This is the honest trade-off of the current design.

## 10. Known limitations and future improvements

Everything in this section describes the code **as it exists**, verified against the sources. Each entry states the current behavior and, where the fix is known, what it would take.

### 10.1 Correctness gaps

| Current behavior | Fix sketch |
|---|---|
| `unsetenv` is fully implemented but the dispatcher has no branch for it — typing `unsetenv VAR` falls through to the external executor and fails, even though `.help` and `which` advertise it | Add one branch mirroring `setenv`: `*env = command_unsetenv(args, *env); return 0;` |
| Bare `echo` prints nothing (not even a newline), because the print loop never runs when there are no arguments | Print a lone `'\n'` when `args[1]` is `NULL` |
| `echo -n Hello world` still ends with a newline — `new_line` is reset to `1` at the end of every iteration, resurrecting the disabled flag (`built_in_commands.c:73`) | Track the `-n` state in a separate variable from the per-argument spacing logic |
| `setenv` appends without replacing: `setenv PATH=...` twice creates two `PATH` entries | Scan for an existing exact-name entry first; replace its value or reuse its slot |
| `command_cd` returns `0` even when `chdir` fails, so failures are invisible to any future exit-status reporting | Return `1` on the failure path |
| `cd` without an argument only prints an error — no `$HOME` fallback, no `cd -` | Resolve `HOME` from the environment; `cd -` would use the currently unused `initial_directory` parameter (Section 4.2) |
| `perror("getline")` on EOF can print a misleading message, because `errno` may be unset when `getline` hits end-of-file | Distinguish EOF from error with `feof(stdin)` before reporting |
| `which` lists `unsetenv` as built-in (it is not dispatched) and omits `.help`/`help` (they are) | Synchronize the table with the dispatcher once `unsetenv` is wired |

### 10.2 Parser limitations

- No quotation marks, escaped characters, pipes, or redirections — arguments cannot contain spaces, and `echo "a  b"` collapses the spacing.
- No globbing (`ls *.c` is passed literally) and no comment syntax.
- Hard cap of ~1023 tokens per command (`MAX_INPUT_SIZE` slots); overlong commands are rejected with `Too many command tokens` — a graceful failure, but a cap.

### 10.3 Process model

- **No signal handling.** Ctrl+C at the prompt terminates the shell itself (default disposition). Real shells install a `SIGINT` handler at the prompt and forward the signal to foreground children — the `WIFSIGNALED` handling in Section 7.1 already receives the evidence when a child is killed.
- **No background execution** (`&`), no job control. `waitpid` always blocks.
- **No `$?`.** `executors` already returns the child's exit status (including the `128 + signal` convention), but `run_shell` discards it — only the consumer is missing.
- `exit`/`quit` terminate via `exit(EXIT_SUCCESS)` and skip the shutdown `free()` calls. Harmless — the OS reclaims everything at process exit — but it is a shortcut, not a clean unwind.

### 10.4 Environment handling

- Each successful `setenv`/`unsetenv` leaks the previous environment array (bounded; Section 9). The proper fix is ownership bookkeeping — marking which strings are mine — or migrating to `setenv(3)`.
- The process environment (`environ`) is never updated, so library functions like `getenv()` see a stale view after the first `setenv` (Section 2.3). This is by design inside the shell, but it is a real divergence to be aware of.
- `echo $VAR` has no `${VAR}` brace form and no special variables (`$?`, `$$`).

### 10.5 Structural and portability items

- **Two parallel `PATH` searchers** — `find_command_in_path` (Section 6.6) and `get_path` + `split_paths` + the loop in `child_process` (Section 7). They also differ in rigor: the `which` version silently truncates over-long candidates; the executor version skips them via the `snprintf` return check. One shared helper with the stricter behavior would remove the duplication.
- `MAX_INPUT_SIZE` serves as both the token-array capacity and the path-buffer size — two concepts wearing one constant (Section 3.4).
- `find_command_in_path` is forward-declared inside `built_in_commands.c` instead of declared in the shared header (Section 3.5).
- The `access()`-then-`execve()` sequence has a theoretical race (Section 7.5).
- Developed and tested on Ubuntu (POSIX). On Windows/MSYS2 UCRT64, `<sys/wait.h>` was not available (`shell_config.h:17`); a strict `-std=c99 -pedantic` build would also need feature-test macros for `getline`/`strdup` (Section 3.3).

## Appendix A. POSIX vs. ISO C functions used

| Source | Functions |
|---|---|
| ISO C | `printf`, `perror`, `malloc`, `free`, `exit`, `strcmp`, `strncmp`, `strlen`, `strchr`, `strtok`, `snprintf` |
| POSIX (`<unistd.h>`) | `getcwd`, `chdir`, `access`, `fork`, `execve`, `_exit`, `X_OK` |
| POSIX (`<sys/wait.h>`) | `waitpid`, `WIFEXITED`, `WIFSIGNALED`, `WEXITSTATUS`, `WTERMSIG` |
| POSIX (other) | `getline` (stdio, POSIX.1-2008), `strdup` (string) |
| Types | `size_t`, `ssize_t`, `pid_t`, `FILE` — `ssize_t` and `pid_t` are POSIX types |

The ISO C list is why the code compiles anywhere with a standard library; the POSIX list is why it is a *Unix* shell. A strictly conforming `-std=c99` build may require feature-test macros to expose the POSIX declarations (Section 3.3).

## Appendix B. Glossary

| Term | Meaning |
|---|---|
| Token | One individual component of a command, produced by splitting the input on delimiters |
| Tokenization | The act of splitting an input string into a token array |
| REPL | Read–eval–print loop — the prompt → input → execute cycle of an interactive shell |
| Built-in command | A command executed by the shell process itself, not by launching a program |
| External command | A separate executable program located via `PATH` and run in a child process |
| Process | A program in execution, with its own memory, file descriptors, and working directory |
| PID | Process identifier — the number the OS assigns to a running process |
| `fork()` | System call that duplicates the calling process; called once, returns twice |
| `execve()` | System call that replaces the program running in the calling process; on success it never returns |
| `waitpid()` | Blocks the parent until a chosen child terminates and collects its status |
| Environment variable | A `"NAME=value"` string passed to processes; the collection is a NULL-terminated array |
| `environ` | The C library's global process-environment array, distinct from the shell's private copy |
| NULL terminator | A trailing `NULL` pointer marking the end of a pointer array (sentinel) |
| `'\0'` | The terminating byte of a C string |
| Memory leak | Dynamically allocated memory that is no longer reachable and never freed |
| Dangling pointer | A pointer to memory whose lifetime has ended; using it is undefined behavior |
| Out-parameter | A parameter passed by address so the callee can report a result back to the caller |
| Translation unit | One `.c` file after preprocessing — the unit the compiler and the linker's symbol resolution work with |
| Header guard | The `#ifndef`/`#define`/`#endif` construct that makes a header safe to include repeatedly |
| Object file | A `.o` file — compiled machine code, not yet linked into an executable |
| Linker | The tool that combines object files into the final executable |
| EOF | End-of-file — no more input available (Ctrl+D at an empty prompt) |
| TOCTOU | Time-of-check-to-time-of-use — a race between checking a condition and acting on it |
