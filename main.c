#include <stdio.h> // main entry point, printf
#include <string.h> // strcat
#include <stdlib.h> // malloc
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h> // O_ definitions
#include "syscall.h"


// enums
#define ERR_MALLOC 1
#define ERR_FGETS 2
#define ERR_WRONGARG 3
#define ERR_EXECFAIL 4

#define SHELL_TYPE_LOCAL 0
#define SHELL_TYPE_CLIENT 1
#define SHELL_TYPE_SERVER 2

// windows dev environment incomplete imports workaround
#ifndef _SC_HOST_NAME_MAX 
#define _SC_HOST_NAME_MAX 255
#endif

// configurables
#define SHELL_SOCKNAME_MAX 255
#define SHELL_USERINPUT_MAX 4096
#define SHELL_HISTORY_MAX 20

#define PROMPT_DELIMITER '|'
#define PROMPT_HOSTNAME_MAX _SC_HOST_NAME_MAX

#define PARG_NTYPE_FINISHED 0
#define PARG_NTYPE_SEMICOLON 1
#define PARG_NTYPE_PIPE 2

#define IS_PIPE_BOTH 2
#define IS_PIPE_RIGHT 1
#define IS_PIPE_NONE 0
#define IS_PIPE_LEFT -1

#define PIPE_READ 0
#define PIPE_WRITE 1

const char help[] = "\n\
[seeHell]\n\
\tInteractive C-Shell for SPAASM\n\
\tOndrej Spanik 2022 (c) iairu.com\n\
- Arguments:\n\
\t-p <number>   Runs socketed on the given port number\n\
\t-u <sockname> Runs socketed on the given AF_UNIX socket name\n\
\t              If neither -p nor -u are specified, shell runs unsocketed\n\
\t-c            Switches from server to client (with -p, -u specified)\n\
\t-h            Displays help (this message)\n\
- Commands:\n\
\thalt          Halts the shell execution completely\n\
\thelp          Displays help (this message)\n\
";

// man 3 exec
extern char **environ;

// processes supported arguments into respective variables
// sizeof(shell_sockname) => shell_sockname_size for constant-sized char arrays
// returns 1 on error, 0 if no error
char processArgs(int argc, char* argv[], char* shell_type, int* shell_port, char* shell_sockname, unsigned int shell_sockname_size) {
    int i;
    char flag = '\0';
    for (i = 1; i < argc; i++) {
        switch(flag) {
            case '\0': // get the next arg flag
                if      (strcmp(argv[i], "-p") == 0) flag = 'p'; // takes a value
                else if (strcmp(argv[i], "-u") == 0) flag = 'u'; // takes a value
                else if (strcmp(argv[i], "-c") == 0) {flag = 'c'; i--;} // doesn't take values
                else if (strcmp(argv[i], "-h") == 0) {flag = 'h'; i--;} // doesn't take values
                else    {fprintf(stderr, "Unrecognized argument [%s].\n", argv[i]); return 1; }
                break;
            case 'p': // set port (and server if not flagged as a client)
                (*shell_type) = ((*shell_type) == SHELL_TYPE_LOCAL) ? SHELL_TYPE_SERVER : (*shell_type);
                if (((*shell_port) = atoi(argv[i])) == 0) {
                    fprintf(stderr, "Argument [-p] must be followed by a non-zero port number.\n");
                    return 1;
                }
                flag = '\0';
                break;
            case 'u': // set socket name (and server if not flagged as a client)
                (*shell_type) = ((*shell_type) == SHELL_TYPE_LOCAL) ? SHELL_TYPE_SERVER : (*shell_type);
                strncpy(shell_sockname, argv[i], shell_sockname_size); // _todo no checks are made for socket name input
                flag = '\0';
                break;
            case 'c': // flag as a client
                (*shell_type) = SHELL_TYPE_CLIENT;
                flag = '\0';
                break;
            case 'h': // print help
                printf("%s\n", help);
                flag = '\0';
                break;
        }
    }
    if (flag != '\0') {
        fprintf(stderr, "Argument [-%c] missing value input afterwards.\n", flag);
        return 1;
    }
    if ((*shell_type) == SHELL_TYPE_CLIENT && (*shell_port) == 0 && shell_sockname[0] == '\0') { 
        fprintf(stderr, "Argument [-c] must be used alongside a port number or socket name.\n");
        return 1;
    }
    return 0;
}

// gets the prompt using syscalls roughly as follows: TIME GETPWUID(GETUID)@HOSTNAME:
void printPrompt() {
    // unix time
    time_t utime[1];
    utime[0] = sc_time();

    // human readable time
    struct tm *htime;
    htime = localtime(utime);

    // user name
    char *name = getpwuid(sc_getuid())->pw_name;
    
    // host name
    char hostname[PROMPT_HOSTNAME_MAX];
    gethostname(hostname, PROMPT_HOSTNAME_MAX);

    // build and output an up-to-date prompt
    printf("%02d:%02d %s@%s%c ", 
        htime->tm_hour, 
        htime->tm_min,
        name,
        hostname,
        PROMPT_DELIMITER
        );
}

// remove spaces from the left
char *ltrim(char* str) {
    while((*str) == ' ') str++;
    return str;
}

// remove spaces from the right
char *rtrim(char* str) {
    char *end = str + strlen(str);
    while((*(--end)) == ' ');
    *(++end) = '\0';
    return str;
}

// remove spaces from both left and right
char *trim(char* str) {
    return ltrim(rtrim(str)); 
}

// set the current working directory
// verifiable using external ls or pwd
void changedir(char* arg) {
    // check for input and trim arg
    // also if no input => cd to HOME directory
    if (arg == NULL || (arg = trim(arg))[0] == '\0') {
        char *homedir = getpwuid(sc_getuid())->pw_dir;
        // printf("[%s]\n", homedir);
        if (chdir(homedir) != 0) perror("cd error");
    } else {
        // process the user input as a directory location
        // printf("[%s]\n", arg);
        if (chdir(arg) != 0) perror("cd error");
    }
}

// parse internal user input as arguments for external command execution
// all parameters except the first are output
char **parseArgs(char* input, int* _argc, char** _redir_in, char** _redir_out, char* _next_type, char** _next_input) {
    char **args = NULL;
    char buffer[SHELL_USERINPUT_MAX];
    memset(buffer, '\0', sizeof(buffer));
    
    (*_redir_in) = NULL;
    (*_redir_out) = NULL;
    (*_next_type) = PARG_NTYPE_FINISHED;
    (*_next_input) = NULL;
    
    char *bp = NULL;
    char *ip = NULL;
    char **ap = NULL;
    char quote = 0;
    char waschar = 0;
    char escaped = 0;
    char commented = 0;
    char redirected = 0;
    char nextarg = 0;
    char **redir;
    int count = 0;

    // count the number of arguments
    ip = input;
    ip--;
    while((*++ip) != '\0' && !commented && !nextarg) {
        switch(*ip) {
            case '\\':
                if (!escaped) {
                    escaped = 1;
                    continue;
                }
            case ';':
            case '|':
                if (!escaped) {
                    nextarg = 1;
                    continue;
                }
            case '<':
            case '>':
                if (!escaped) {
                    redirected = 1;
                    continue;
                }
            case '#':
                if (!escaped) {
                    commented = 1;
                    continue;
                }
            case '\"':
                if (!escaped) {
                    quote = !quote;
                    if (quote) continue; // don't count the quote as arg character
                }
            case ' ':
                if (!escaped) {
                    if (!quote && waschar) {
                        // end of a non-empty argument within or outside quotes
                        if (!redirected) count++;
                        waschar = 0;
                        redirected = 0;
                        continue;
                    }
                    if (!quote) continue; // don't count space outside quotes as arg character
                }
            default:
                // any non-quote characters, except spaces outside quotes
                waschar = 1;
                escaped = 0;
        }
    }
    if (quote) {
        fprintf(stderr, "Matching quote not found.\n");
        return NULL;
    }
    if (waschar) {
        // if there have been any arguments
        // count the last unqoted argument in 
        // quoted args have already been counted on quotes
        if (!redirected) count++;
    }

    // printf("arg count: %d\n", count);

    // malloc buffers for the number of arguments
    // freed outside function after use incl. all children to be malloc'd below
    args = malloc((count + 1) * sizeof(char*));
    if (args == NULL) {
        fprintf(stderr, "Memory allocation error.\n");
        return NULL;
    }
    args[count] = (char *) NULL; // requirement for exec to NULL-terminate the pointer array
    (*_argc) = count;

    // save the arguments
    waschar = 0;
    escaped = 0;
    commented = 0;
    redirected = 0;
    nextarg = 0;
    bp = buffer;
    ip = input;
    ap = args;
    ip--;
    while((*++ip) != '\0' && !commented && !nextarg) {
        switch(*ip) {
            case '\\':
                if (!escaped) {
                    escaped = 1;
                    continue;
                }
            case ';':
                if (!escaped) {
                    nextarg = 1;
                    (*_next_type) = PARG_NTYPE_SEMICOLON;
                    (*_next_input) = ip + 1;
                    continue;
                }
            case '|':
                if (!escaped) {
                    nextarg = 1;
                    (*_next_type) = PARG_NTYPE_PIPE;
                    (*_next_input) = ip + 1;
                    continue;
                }
            case '<':
                if (!escaped) {
                    redirected = 1;
                    continue;
                }
            case '>':
                if (!escaped) {
                    redirected = 2;
                    continue;
                }
            case '#':
                if (!escaped) {
                    commented = 1;
                    continue;
                }
            case '\"': 
                if (!escaped) {
                    quote = !quote;
                    if (quote) continue; // don't count the quote as arg character
                }
            case ' ':
                if (!escaped) {
                    if (!quote && waschar) {
                        // end of a non-empty argument within or outside quotes
                        (*bp) = '\0'; // end buffer
                        // printf("%s\n", buffer);
                        if (!redirected) {
                            (*ap) = malloc((strlen(buffer) + 1) * sizeof(char));
                            if ((*ap) == NULL) {
                                // _todo free previous memory
                                fprintf(stderr, "Memory allocation error.\n");
                                return NULL;
                            }
                            strcpy((*ap++), buffer);
                        } else {
                            redir = (redirected == 1) ? _redir_in : _redir_out; 
                            if ((*redir) == NULL)
                                (*redir) = malloc(SHELL_USERINPUT_MAX * sizeof(char));
                            if ((*redir) == NULL) {
                                // _todo free previous memory
                                fprintf(stderr, "Memory allocation error.\n");
                                return NULL;
                            }
                            strcpy((*redir), buffer);
                            redirected = 0;
                        }
                        bp = buffer; // reset buffer position
                        waschar = 0;
                        continue;
                    }
                    if (!quote) continue; // don't count space outside quotes as arg character
                }
            default:
                // any non-quote characters, except spaces outside quotes
                waschar = 1;
                (*bp++) = (*ip); // add to buffer (buffer overflow is protected outside function by max uinput size)
                escaped = 0;
        }
    }
    // if (quote) {...} // already checked during counting
    if (waschar) {
        // if there have been any arguments
        // count the last unqoted argument in 
        // quoted args have already been counted on quotes
        (*bp) = '\0'; // end buffer
        // printf("%s\n", buffer);
        if (!redirected) {
            (*ap) = malloc((strlen(buffer) + 1) * sizeof(char));
            if ((*ap) == NULL) {
                // _todo free previous memory
                fprintf(stderr, "Memory allocation error.\n");
                return NULL;
            }
            strcpy((*ap), buffer);
        } else {
            redir = (redirected == 1) ? _redir_in : _redir_out; 
            if ((*redir) == NULL)
                (*redir) = malloc(SHELL_USERINPUT_MAX * sizeof(char));
            if ((*redir) == NULL) {
                // _todo free previous memory
                fprintf(stderr, "Memory allocation error.\n");
                return NULL;
            }
            strcpy((*redir), buffer);  
            redirected = 0;
        }
    }

    // consider this to be the last command if there is nothing left after ; or |
    if ((*_next_type) == PARG_NTYPE_SEMICOLON || (*_next_type) == PARG_NTYPE_PIPE) {
        if (strlen(trim(*_next_input)) == 0) {
            if ((*_next_type) == PARG_NTYPE_PIPE) {
                fprintf(stderr, "No command after pipe.\n");
                (*_next_type) = PARG_NTYPE_FINISHED;
                return NULL;
            }
            (*_next_type) = PARG_NTYPE_FINISHED;
        }
    }

    return args;
}

// free arguments retreived from parseArgs
void freeArgs(char **args, int argc, char *redir_in, char *redir_out) {
    int i;
    for(i = 0; i <= argc; i++) { // incl. NULL-terminated pointer at the end 
        free(args[i]);
        // printf("(freed arg %d)\n", i);
    }
    free(args);
    // printf("(freed args)\n");
    if (redir_in != NULL) free(redir_in);
    if (redir_out != NULL) free(redir_out);
        
}

// handle child process behavior after successful forking
void handleChild(char *const args[], int argc, 
                 char *redir_in, char *redir_out, 
                 char is_pipe, 
                 int *pipe_left_read, int *pipe_left_write, 
                 int *pipe_right_read, int *pipe_right_write) {
                     
    if (argc == 0) return;

    // open file-redirected input and output files each exists
    // replace STDIN/STDOUT streams with these files
    // otherwise if PIPES found, replace STDIN/STDOUT streams with PIPE_READ/PIPE_WRITE
    if (redir_in != NULL) {
        int in_fd = open(redir_in, O_RDONLY);
        if (in_fd < 0) {
            perror("Failed to open input file");
            return;
        }
        if (dup2(in_fd, STDIN_FILENO) == -1) {
            perror("Failed to redirect STDIN to input file");
            return;
        }
        close(in_fd);
    } else if (is_pipe == IS_PIPE_LEFT || is_pipe == IS_PIPE_BOTH) { // only read from left
        close((*pipe_left_write)); (*pipe_left_write) = -1;
        if (dup2((*pipe_left_read), STDIN_FILENO) == -1) {
            perror("Failed to redirect STDIN to the read end of the left pipe");
            return;
        }
        close((*pipe_left_read)); (*pipe_left_read) = -1;
    }

    if (redir_out != NULL) {
        int out_fd = open(redir_out, O_WRONLY | O_CREAT, 0644);
        if (out_fd < 0) {
            perror("Failed to open output file");
            return;
        }
        if (dup2(out_fd, STDOUT_FILENO) == -1) {
            perror("Failed to redirect STDOUT to output file");
            return;
        }
        close(out_fd);
    } else if (is_pipe == IS_PIPE_RIGHT || is_pipe == IS_PIPE_BOTH) { // only write to right
        close((*pipe_right_read)); (*pipe_right_read) = -1;
        if (dup2((*pipe_right_write), STDOUT_FILENO) == -1) {
            perror("Failed to redirect STDOUT to the write end of the right pipe");
            return;
        }
        close((*pipe_right_write)); (*pipe_right_write) = -1;
    }

    

    // man 3 exec
    if (execvp(args[0], args) == -1) { // execvp takes the extern char **environ variable
        perror("Failed to execute.");
        return;
    } 
}

char **allocHistory() {
    int i;
    char **history = malloc(SHELL_HISTORY_MAX * sizeof(char *));
    if (history == NULL) {
        fprintf(stderr, "Memory allocation error (history).\n");
        return NULL;
    }
    for (i = 0; i < SHELL_HISTORY_MAX; i++) {
        history[i] = malloc(SHELL_USERINPUT_MAX * sizeof(char));
        if (history[i] == NULL) {
            fprintf(stderr, "Memory allocation error (history).\n");
            return NULL;
        }
    }
    return history;
}

void pushHistory(char **history, const char *uinput) {
    int i;
    char *moved = history[SHELL_HISTORY_MAX - 1];
    for (i = SHELL_HISTORY_MAX - 1; i > 0; i--)
        history[i] = history[i - 1];
    history[0] = moved;
    strcpy(history[0], uinput);
}

void printHistory(char **history) {
    int i;
    int j = 0;
    for (i = SHELL_HISTORY_MAX - 1; i >= 0; i--) {
        if (history[i][0] == '\0') continue;
        printf("  %d\t%s\n", ++j, history[i]);
    }
}

void freeHistory(char **history) {
    freeArgs(history, SHELL_HISTORY_MAX - 1, NULL, NULL);
}

char *fgetskb(char *buffer, int bufsize, FILE *stream) {
    char c;
    int i = 0;
    while (i < bufsize - 1) { // if the first value is esc
        c = getc(stream);
        if (c == EOF) {
            return NULL;
        } else if (c == 27 || c == '^') {
            if (c == '^') {
                // my shell behavior
                if ((c = getc(stream)) != '[') {
                    ungetc(c, stream);
                    buffer[i++] = '^';
                    continue;
                }
                if ((c = getc(stream)) != '[') {
                    ungetc(c, stream);
                    ungetc('[', stream);
                    buffer[i++] = '^';
                    continue;
                }
            } else if (c == 27) {
                // skip the [ from https://stackoverflow.com/a/11432632
                if ((c = getc(stream)) != '[') {
                    ungetc(c, stream);
                    buffer[i++] = 27;
                    continue;
                }
            }
            i = 0;
            getc(stream); // skip the [ from https://stackoverflow.com/a/11432632
            switch(getc(stream)) { // the real value
                case 'A':
                    // code for arrow up
                    printf("up\n");
                    break;
                case 'B':
                    // code for arrow down
                    printf("down\n");
                    break;
                case 'C':
                    // code for arrow right
                    printf("right\n");
                    break;
                case 'D':
                    // code for arrow left
                    printf("left\n");
                    break;
            }
        } else if (c == '\n' || c == '\r') {
            break;
        } else {
            buffer[i++] = c;
        }
    }
    buffer[i++] = '\n';
    buffer[i] = '\0';
    return buffer;
}

// --------------------------------------
// --------------------------------------
// --------------------------------------
// --------------------------------------

int main(int argc, char* argv[]) {
    // argument handling
    char shell_type = SHELL_TYPE_LOCAL;
    int shell_port = 0;
    char shell_sockname[SHELL_SOCKNAME_MAX];
    memset(shell_sockname, '\0', sizeof(shell_sockname));
    if (processArgs(argc, argv, &shell_type, &shell_port, shell_sockname, sizeof(shell_sockname))) return ERR_WRONGARG;

    // socket preparation
    // todo if server && port: port availability check
    // todo if server && socket name: socket name availability check

    // inform the user
    printf("Welcome to seeHell, running as a [%s].\n\n", 
        (shell_type == SHELL_TYPE_LOCAL) ? "local self" :
        (shell_type == SHELL_TYPE_CLIENT) ? "local unix client" :
        (shell_type == SHELL_TYPE_SERVER) ? "local unix server" : "???"
        );

    // user input buffer
    char *uinput = malloc(SHELL_USERINPUT_MAX * sizeof(char));
    if (uinput == NULL) {
        fprintf(stderr, "Memory allocation error (user input).\n");
        return ERR_MALLOC;
    }

    // command history buffers
    char **history = allocHistory();
    if (history == NULL) return ERR_MALLOC;

    // interactive shell until "halt" encountered
    while (1 == 1) {
        // show prompt
        printPrompt();
        
        // user input
        rewind(stdin);
        if (fgetskb(uinput, SHELL_USERINPUT_MAX, stdin) == NULL) return ERR_FGETS;
        rewind(stdin);                        // remove any trailing STDIN
        uinput[strcspn(uinput, "\n")] = '\0'; // remove trailing newline STDOUT
        pushHistory(history, uinput);         // add to history
        // printf("[%s]\n", uinput);

        // built-in command execution
        // _todo argument parsing for built-ins (no use-case found for now)
        char builtin = 1;
        if      (strcmp(uinput, "halt") == 0) break; // break out of the interactive shell
        else if (strcmp(uinput, "quit") == 0) break; // todo quit connection (client sends quit to server,
        // todo server closes connection on socket, client realizes the connection is closed as planned and halts)
        else if (strlen(uinput) >= 3 && strncmp(uinput, "cd ", 3) == 0) changedir(uinput + 3); // cd to arg
        else if (strcmp(uinput, "cd") == 0) changedir(NULL); // cd to home on no args
        else if (strcmp(uinput, "help") == 0) printf("%s\n", help); // print help
        else if (strcmp(uinput, "history") == 0) printHistory(history); // print history
        else    builtin = 0;
        if (builtin) continue;
        // else    fprintf(stderr, "Unrecognized command.\n");


        // external command execution: handle each ';' and '|' delimited command
        char shell_next_type = PARG_NTYPE_SEMICOLON; // default behavior for first run  as if next command after semicolon
        char *shell_next_uinput = uinput; // give the full user input and move behind processed part on each execution
        char is_pipe = IS_PIPE_NONE; // if the last run was piped as input, the next one has to receive pipe output
        int fd_pipe_l[2] = {-1, -1}; // {read, write} pair
        int fd_pipe_r[2] = {-1, -1}; // {read, write} pair
        while(shell_next_type != PARG_NTYPE_FINISHED) {

            // argument preparation for program execution
            int shell_argc = 0;
            char *shell_redir_in, *shell_redir_out;
            char *shell_uinput = shell_next_uinput;
            char **shell_args = parseArgs(shell_uinput, &shell_argc, &shell_redir_in, &shell_redir_out, &shell_next_type, &shell_next_uinput);
            if (shell_args == NULL) continue; // error parsing arguments, command can't be processed
            
            // for (i = 0; i < shell_argc; i++) printf("%s\n", shell_args[i]);
            // if (shell_redir_in != NULL) printf("< [%s]\n", shell_redir_in);
            // if (shell_redir_out != NULL) printf("> [%s]\n", shell_redir_out);

            // pipe preparation if pipe found on the right side of this command
            if (shell_next_type == PARG_NTYPE_PIPE) {
                // Create a new pipe
                if (pipe(fd_pipe_r) != 0) {
                    perror("Pipe error");
                    freeArgs(shell_args, shell_argc, shell_redir_in, shell_redir_out); 
                    break;
                }
                // There may be either the new pipe on right or an already existing one on left + the new one
                is_pipe = (is_pipe == IS_PIPE_LEFT) ? IS_PIPE_BOTH : IS_PIPE_RIGHT;
            }

            // printf("pipes before fork: left[read %d, write %d] right[read %d, write %d]\n", 
            //         fd_pipe_l[PIPE_READ], fd_pipe_r[PIPE_WRITE], fd_pipe_r[PIPE_READ], fd_pipe_r[PIPE_WRITE]);

            // fork execution
            pid_t pid;
            int wstatus;
            pid = fork(); // man 2 fork
            if (pid == -1) {
                perror("Fork error"); 
                freeArgs(shell_args, shell_argc, shell_redir_in, shell_redir_out); 
                break;
            } else if (pid == 0) {
                // child process

                handleChild(shell_args, shell_argc, shell_redir_in, shell_redir_out, is_pipe, 
                            &(fd_pipe_l[PIPE_READ]), &(fd_pipe_l[PIPE_WRITE]),
                            &(fd_pipe_r[PIPE_READ]), &(fd_pipe_r[PIPE_WRITE])); 
                return ERR_EXECFAIL;

            } else {
                // parent process
                // pid is set to child pid

                if (is_pipe == IS_PIPE_LEFT || is_pipe == IS_PIPE_BOTH) {
                    // close left-side pipes for parent process
                    close(fd_pipe_l[PIPE_READ]); fd_pipe_l[PIPE_READ] = -1;
                    close(fd_pipe_l[PIPE_WRITE]); fd_pipe_l[PIPE_WRITE] = -1;
                    is_pipe = (is_pipe == IS_PIPE_BOTH) ? IS_PIPE_RIGHT : IS_PIPE_NONE;
                }
                
                // must wait for child to finish executing
                // then resume interactive shell
                do {
                    // wait(&wstatus); // man 2 wait
                    waitpid(pid, &wstatus, WUNTRACED);
                } while (!WIFEXITED(wstatus) && !WIFSIGNALED(wstatus));
                // printf("child [%d] exited with status [%d]\n", pid, wstatus);

                if (is_pipe == IS_PIPE_RIGHT) { // move the pipe for next command from right to left
                    fd_pipe_l[PIPE_READ] = fd_pipe_r[PIPE_READ]; fd_pipe_r[PIPE_READ] = -1;
                    fd_pipe_l[PIPE_WRITE] = fd_pipe_r[PIPE_WRITE]; fd_pipe_r[PIPE_WRITE] = -1;
                    is_pipe = IS_PIPE_LEFT; // now on the left of the next command
                }
            }

            // if (shell_next_type != PARG_NTYPE_FINISHED) {
            //     printf("NEXT [%c]\n", (shell_next_type == PARG_NTYPE_SEMICOLON) ? ';' : '|');
            //     printf("with [%s]\n", shell_next_uinput);
            // }

            // free arguments used in program execution
            freeArgs(shell_args, shell_argc, shell_redir_in, shell_redir_out);   
            // printf("freed shell_...\n");
        }
     
    };

    // free buffers
    free(uinput);
    // printf("freed uinput\n");
    freeHistory(history);
    // printf("freed history\n");

    return 0;
}
