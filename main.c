#include <stdio.h> // main entry point, printf
#include <string.h> // strcat
#include <stdlib.h> // malloc
#include <time.h>
#include <unistd.h>
#include "syscall.h"


// enums
#define ERR_MALLOC 1
#define ERR_FGETS 2
#define ERR_WRONGARG 3

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
                strncpy(shell_sockname, argv[i], shell_sockname_size); // todo no checks are made for socket name input
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

char *ltrim(char* str) {
    while((*str) == ' ') str++;
    return str;
}

char *rtrim(char* str) {
    char *end = str + strlen(str);
    while((*(--end)) == ' ');
    *(++end) = '\0';
    return str;
}

char *trim(char* str) {
    return ltrim(rtrim(str)); 
}

void changedir(char* arg) {
    // check for input and trim arg
    // also if no input => cd to HOME directory
    if (arg == NULL || (arg = trim(arg))[0] == '\0') {
        char *homedir = getpwuid(sc_getuid())->pw_dir;
        printf("[%s]\n", homedir);
        if (chdir(homedir) != 0) perror("cd error");
    } else {
        // process the user input as a directory location
        printf("[%s]\n", arg);
        if (chdir(arg) != 0) perror("cd error");
    }
}

char **splitArgs(char* input, int* count_out) {
    char **args = NULL;
    char buffer[SHELL_USERINPUT_MAX];
    memset(buffer, '\0', sizeof(buffer));
    char *bp = NULL;
    char *ip = NULL;
    char **ap = NULL;
    char quote = 0;
    char waschar = 0;
    int count = 0;

    // count the number of arguments
    ip = input;
    ip--;
    while((*++ip) != '\0') {
        switch(*ip) {
            case '\"': 
                quote = !quote;
                if (quote) continue; // don't count the quote as arg character
            case ' ':
                if (!quote && waschar) {
                    // end of a non-empty argument within or outside quotes
                    count++;
                    waschar = 0;
                    continue;
                }
                if (!quote) continue; // don't count space outside quotes as arg character
            default:
                // any non-quote characters, except spaces outside quotes
                waschar = 1;
        }
    }
    if (quote) {
        fprintf(stderr, "Matching quote not found.\n");
        return NULL;
    }
    if (ip != input && *(ip - 1) != '\"') {
        // if there have been any arguments
        // count the last unqoted argument in 
        // quoted args have already been counted on quotes
        count++;
    }
    waschar = 0;
    // printf("arg count: %d\n", count);

    // malloc buffers for the number of arguments
    // todo freed outside function after use incl. all children to be malloc'd below
    args = malloc(count * sizeof(char*));
    if (args == NULL) return NULL;
    (*count_out) = count;

    // save the arguments
    bp = buffer;
    ip = input;
    ap = args;
    ip--;
    while((*++ip) != '\0') {
        switch(*ip) {
            case '\"': 
                quote = !quote;
                if (quote) continue; // don't count the quote as arg character
            case ' ':
                if (!quote && waschar) {
                    // end of a non-empty argument within or outside quotes
                    (*bp) = '\0'; // end buffer
                    // printf("%s\n", buffer);
                    (*ap) = malloc((strlen(buffer) + 1) * sizeof(char));
                    if ((*ap) == NULL) {
                        // todo free previous memory
                        return NULL;
                    }
                    strcpy((*ap++), buffer);
                    bp = buffer; // reset buffer position
                    waschar = 0;
                    continue;
                }
                if (!quote) continue; // don't count space outside quotes as arg character
            default:
                // any non-quote characters, except spaces outside quotes
                waschar = 1;
                (*bp++) = (*ip); // add to buffer (buffer overflow is protected outside function by max uinput size)
        }
    }
    // if (quote) {...} // already checked during counting
    if (ip != input && *(ip - 1) != '\"') {
        // if there have been any arguments
        // count the last unqoted argument in 
        // quoted args have already been counted on quotes
        (*bp) = '\0'; // end buffer
        // printf("%s\n", buffer);
        (*ap) = malloc((strlen(buffer) + 1) * sizeof(char));
        if ((*ap) == NULL) {
            // todo free previous memory
            return NULL;
        }
        strcpy((*ap), buffer);
    }

    return args;
}

void freeArgs(char **args, int argc) {
    int i;
    for(i = 0; i < argc; i++)
        free(args[i]);
    free(args);
}

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

        // command execution
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

        // todo non-builtin commands are looked up as executables
        // todo - split executable from arguments
        // todo - check whether non ./ executable is in PATH and ./ is in pwd
        // todo - if no executable found print to stderr

        // argument preparation for program execution
        int shell_argc = 0;
        char **shell_args = splitArgs(uinput, &shell_argc);
        if (shell_args == NULL) {
            fprintf(stderr, "Memory allocation error.\n");
            return ERR_MALLOC;
        }
        for (int i = 0; i < shell_argc; i++) printf("%s\n", shell_args[i]);

        // free arguments used in program execution
        freeArgs(shell_args, shell_argc);

    };

    // free buffers
    free(uinput);

    return 0;
}