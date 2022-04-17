#include <stdio.h> // main entry point, printf
#include <string.h> // strcat
#include <stdlib.h> // malloc
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h> // O_ definitions
// #include <sys/wait.h>
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

#define PROMPT_DELIMITER '|'
#define PROMPT_HOSTNAME_MAX _SC_HOST_NAME_MAX

#define PARG_NTYPE_FINISHED 0
#define PARG_NTYPE_SEMICOLON 1
#define PARG_NTYPE_PIPE 2

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
                    if ((*(ip + 1)) != '\0') {
                        (*_next_type) = PARG_NTYPE_SEMICOLON;
                        (*_next_input) = ip + 1;
                    }
                    continue;
                }
            case '|':
                if (!escaped) {
                    nextarg = 1;
                    if ((*(ip + 1)) != '\0') {
                        (*_next_type) = PARG_NTYPE_PIPE;
                        (*_next_input) = ip + 1;
                    }
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

    return args;
}

// free arguments retreived from parseArgs
void freeArgs(char **args, int argc, char *redir_in, char *redir_out) {
    int i;
    for(i = 0; i < argc + 1; i++) // incl. NULL-terminated pointer at the end
        free(args[i]);
    free(args);
    if (redir_in != NULL) free(redir_in);
    if (redir_out != NULL) free(redir_out);
        
}

// handle child process behavior after successful forking
void handleChild(char *const args[], int argc, char *redir_in, char *redir_out) {
    if (argc == 0) return;

    // open file-redirected input and output files each exists
    // replace STDIN/STDOUT streams with these files
    if (redir_in != NULL) {
        int in_fd = open(redir_in, O_RDONLY);
        if (in_fd < 0) {
            perror("Failed to open input file.");
            return;
        }
        if (dup2(in_fd, 0) == -1) fprintf(stderr, "STDIN Input dup2 failure.\n");
        close(in_fd);
    }
    if (redir_out != NULL) {
        int out_fd = open(redir_out, O_WRONLY | O_CREAT, 0644);
        if (out_fd < 0) {
            perror("Failed to open output file.");
            return;
        }
        if (dup2(out_fd, 1) == -1) fprintf(stderr, "STDOUT Output dup2 failure.\n");
        close(out_fd);
    }

    // man 3 exec
    execvp(args[0], args); // takes the extern char **environ variable
    // if the execution fails (e.g. program doesnt exist), return will be used outside
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
        fprintf(stderr, "Memory allocation error.\n");
        return ERR_MALLOC;
    }

    // interactive shell until "halt" encountered
    while (1 == 1) {
        // show prompt
        printPrompt();
        
        // user input
        if (fgets(uinput, SHELL_USERINPUT_MAX, stdin) == NULL) return ERR_FGETS;
        rewind(stdin);                        // remove any trailing STDIN
        uinput[strcspn(uinput, "\n")] = '\0'; // remove trailing newline STDOUT
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
        else    builtin = 0;
        if (builtin) continue;
        // else    fprintf(stderr, "Unrecognized command.\n");


        // external command execution: handle each ';' and '|' delimited command
        char shell_next_type = PARG_NTYPE_SEMICOLON; // default behavior for first run  as if next command after semicolon
        char *shell_next_uinput = uinput; // give the full user input and move behind processed part on each execution
        char waspipe = 0; // if the last run was piped as input, the next one has to receive pipe output
        int fd_pipe_l[2] = {-1, -1};
        int fd_pipe_r[2] = {-1, -1};
        while(shell_next_type != PARG_NTYPE_FINISHED) {

            // argument preparation for program execution
            int shell_argc = 0;
            char *shell_redir_in, *shell_redir_out;
            char *shell_uinput = shell_next_uinput;
            char **shell_args = parseArgs(shell_uinput, &shell_argc, &shell_redir_in, &shell_redir_out, &shell_next_type, &shell_next_uinput);
            if (shell_args == NULL) continue; // error parsing arguments, command can't be processed
            // for (int i = 0; i < shell_argc; i++) printf("%s\n", shell_args[i]);
            if (shell_redir_in != NULL) printf("< [%s]\n", shell_redir_in);
            if (shell_redir_out != NULL) printf("> [%s]\n", shell_redir_out);

            // pipe preparation if pipe found on the left side of this command (right side of previous)
            if (waspipe == 1) {
                // Pipe was received during previous run to fd_pipe_l (which means it is already created)
                waspipe = -1;
            }
            // pipe preparation if pipe found on the right side of this command
            if (shell_next_type == PARG_NTYPE_PIPE) {
                // Create a new pipe
                printf("pipe tracing 1\n");
                if (pipe(fd_pipe_r) != 0) perror("pipe error");
                printf("pipe tracing 2\n");
                waspipe = 1;
            }

            // program execution
            pid_t pid;
            int wstatus;
            pid = fork(); // man 2 fork
            char tmpbuffer[SHELL_USERINPUT_MAX];
            memset(tmpbuffer, '\0', sizeof(tmpbuffer));
            switch(pid) {
                case -1: perror("fork error"); break;
                case 0: 
                    // After fork: Child process pipes and further handling incl. execution
                    if (waspipe == -1) { // READING
                        // Left-side pipe
                        // Dup pipe to STDOUT for reading and close for writing
                        printf("pipe tracing 7\n");
                        // if (dup2(fd_pipe_l[0], STDOUT_FILENO) == -1) fprintf(stderr, "STDOUT Pipe dup2 failure.\n");
                        // printf("pipe tracing 7.1\n");
                        // close(fd_pipe_l[0]);
                        // close(fd_pipe_l[1]);
                        printf("pipe tracing 8\n");
                    }
                    if (shell_next_type == PARG_NTYPE_PIPE) { // WRITING
                        // Right-side pipe
                        // Dup pipe to STDIN for writing and close for reading
                        printf("pipe tracing 3\n");
                        if (dup2(fd_pipe_r[1], STDIN_FILENO) == -1) fprintf(stderr, "STDIN Pipe dup2 failure.\n");
                        close(fd_pipe_r[1]);
                        // read(fd_pipe_r[0], &tmpbuffer, sizeof(tmpbuffer));
                        // printf("pipe tracing 3.1 read: [%s]\n", tmpbuffer);
                        // close(fd_pipe_r[0]);
                        printf("pipe tracing 4\n");
                    }
                    handleChild(shell_args, shell_argc, shell_redir_in, shell_redir_out); 
                    return ERR_EXECFAIL;
                default:
                    // After fork: Parent process handling
                    // pid is set to child pid
                    // must wait for child to finish executing
                    // then resume interactive shell
                    // wait(&wstatus); // man 2 wait
                    do {
                        waitpid(pid, &wstatus, WUNTRACED);
                    } while (!WIFEXITED(wstatus) && !WIFSIGNALED(wstatus));
                    if (waspipe == -1) { // FINISHED USAGE
                        // close left-side pipes for parent process (we're finished with them completely)
                        printf("pipe tracing 9\n");
                        // write(fd_pipe_l[0], "testing\0", 8);
                        // read(fd_pipe_l[1], &tmpbuffer, sizeof(tmpbuffer));
                        // printf("pipe tracing 5.1 read: [%s]\n", tmpbuffer);
                        // close(fd_pipe_l[0]);
                        // close(fd_pipe_l[1]);
                        printf("pipe tracing 10\n");
                        if (shell_next_type != PARG_NTYPE_PIPE) waspipe = 0;
                    }
                    if (shell_next_type == PARG_NTYPE_PIPE) { // MOVE FROM WRITING TO READING
                        // move the pipes from right of the currently finished command
                        // to the left of the upcoming command
                        printf("pipe tracing 5\n");
                        read(fd_pipe_l[1], &tmpbuffer, sizeof(tmpbuffer));
                        printf("pipe tracing 5.1 read: [%s]\n", tmpbuffer);
                        // fd_pipe_l[0] = fd_pipe_r[0];
                        // fd_pipe_l[1] = fd_pipe_r[1];
                        printf("pipe tracing 6\n");
                    }
                    // printf("child [%d] exited with status [%d]\n", pid, wstatus);
                
            }

            // if (shell_next_type != PARG_NTYPE_FINISHED) {
            //     printf("NEXT [%c]\n", (shell_next_type == PARG_NTYPE_SEMICOLON) ? ';' : '|');
            //     printf("with [%s]\n", shell_next_uinput);
            // }

            // free arguments used in program execution
            freeArgs(shell_args, shell_argc, shell_redir_in, shell_redir_out);   
        }
     
    };

    // free buffers
    free(uinput);

    return 0;
}