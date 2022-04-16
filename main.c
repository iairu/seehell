#include <stdio.h> // main entry point, printf
#include <string.h> // strcat
#include <stdlib.h> // malloc
#include "syscall.h"

#define ERR_MALLOC 1
#define ERR_FGETS 2

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

int main(int argc, char* argv[]) {
    // get the prompt
    char *prompt = getprompt();
    if (prompt == NULL) return ERR_MALLOC;
    // user input buffer
    char *uinput = malloc(4096);
    if (uinput == NULL) return ERR_MALLOC;

    // interactive shell until "halt" encountered
    while (1 == 1) {
        // show prompt
        printf("%s ", prompt); // todo "%s%s ",prompt,path maybe
        
        // user input
        if (fgets(uinput, 4096, stdin) == NULL) return ERR_FGETS;
        rewind(stdin);                        // remove any trailing STDIN
        uinput[strcspn(uinput, "\n")] = '\0'; // remove trailing newline STDOUT
        // todo split executable from arguments
        // printf("[%s]\n", uinput);

        // command execution
        if      (strcmp(uinput, "halt") == 0) break;
        else if (strcmp(uinput, "help") == 0) printf("%s\n", help);
        else    printf("Unrecognized command.\n");

    };

    // free buffers
    free(prompt);
    free(uinput);

    return 0;
}