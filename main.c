#include <stdio.h> // main entry point, printf
#include <string.h> // strcat
#include <stdlib.h> // malloc
#include "syscall.h"

#define ERR_MALLOC 1
#define ERR_FGETS 2
#define ERR_WRONGARG 3

#define SHELL_TYPE_LOCAL 0
#define SHELL_TYPE_CLIENT 1
#define SHELL_TYPE_SERVER 2

const char help[] = "\n\
Interactive C-Shell for SPAASM\n\
Ondrej Spanik 2022 (c) iairu.com\n\
\n\
Arguments:\n\
\ttodo\n\
\n\
Shell commands:\n\
\thelp - Prints this help message\n\
\thalt - Halts the shell execution completely\n\
";

char *getprompt() {
    // user name
    char *name = getpwuid(sc_getuid())->pw_name;
    
    // host name
    char hostname[256];
    hostname[255] = '\0';
    gethostname(hostname, 254);

    // lengths often reused
    int hostname_len = strlen(hostname);
    int name_len = strlen(name);

    // build the prompt
    char *prompt; // 3 => @ : \0, -1 lebo adresovanie od 0
    if ((prompt = malloc(hostname_len + name_len + 3 - 1)) == NULL)
        return NULL;
    strcpy(prompt, name);
    prompt[name_len] = '@';
    strcpy(prompt + name_len + 1, hostname);
    prompt[name_len + hostname_len + 1] = ':';
    prompt[name_len + hostname_len + 2] = '\0';

    return prompt;
}

// processes supported arguments into respective variables
// sizeof(shell_sockname) => shell_sockname_size for constant-sized char arrays
// returns 1 on error, 0 if no error
char processargs(int argc, char* argv[], char* shell_type, int* shell_port, char* shell_sockname, unsigned int shell_sockname_size) {
    int i;
    char flag = '\0';
    for (i = 1; i < argc; i++) {
        switch(flag) {
            case '\0': // get the next arg flag
                if      (strcmp(argv[i], "-p") == 0) flag = 'p';
                else if (strcmp(argv[i], "-u") == 0) flag = 'u';
                else if (strcmp(argv[i], "-c") == 0) {flag = 'c'; i--;}
                else if (strcmp(argv[i], "-h") == 0) {flag = 'h'; i--;}
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
            case 'h': // print help and exit
                printf("%s\n", help);
                return 0;
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

int main(int argc, char* argv[]) {
    // argument handling
    char shell_type = SHELL_TYPE_LOCAL;
    int shell_port = 0;
    char shell_sockname[255];
    memset(shell_sockname, '\0', sizeof(shell_sockname));
    if (processargs(argc, argv, &shell_type, &shell_port, shell_sockname, sizeof(shell_sockname))) return ERR_WRONGARG;

    // socket preparation
    // todo if server && port: port availability check
    // todo if server && socket name: socket name availability check

    // inform the user
    printf("Welcome to seeHell, running as a [%s].\n\n", 
        (shell_type == SHELL_TYPE_LOCAL) ? "local" :
        (shell_type == SHELL_TYPE_CLIENT) ? "tcp client" :
        (shell_type == SHELL_TYPE_SERVER) ? "tcp server" : "???"
        );

    // // get the prompt
    // char *prompt = getprompt();
    // if (prompt == NULL) return ERR_MALLOC;
    // // user input buffer
    // char *uinput = malloc(4096);
    // if (uinput == NULL) return ERR_MALLOC;

    // // interactive shell until "halt" encountered
    // while (1 == 1) {
    //     // show prompt
    //     printf("%s ", prompt); // todo "%s%s ",prompt,path maybe
        
    //     // user input
    //     if (fgets(uinput, 4096, stdin) == NULL) return ERR_FGETS;
    //     rewind(stdin);                        // remove any trailing STDIN
    //     uinput[strcspn(uinput, "\n")] = '\0'; // remove trailing newline STDOUT
    //     // todo split executable from arguments
    //     // printf("[%s]\n", uinput);

    //     // command execution
    //     if      (strcmp(uinput, "halt") == 0) break;
    //     else if (strcmp(uinput, "help") == 0) printf("%s\n", help);
    //     else    fprintf(stderr, "Unrecognized command.\n");

    // };

    // // free buffers
    // free(prompt);
    // free(uinput);

    return 0;
}