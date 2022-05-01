#include <stdio.h> // main entry point, printf
#include <string.h> // strcat
#include <stdlib.h> // malloc
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h> // O_ definitions    

#include <sys/socket.h>
#include <sys/un.h>       
#include <errno.h>

#define ERR_MALLOC 1
#define ERR_FGETS 2
#define ERR_WRONGARG 3
#define ERR_EXECFAIL 4
#define ERR_SOCKET 5

#define SHELL_USERINPUT_MAX 4096

int main(int ac, char **av, char **en) {
    int s, r;
    fd_set rs;	                                // deskriptory pre select()
    char uinput[SHELL_USERINPUT_MAX];           // uinputer pre prijatu spravu 
    struct sockaddr_un sock_addr;		        // adresa pre soket
    char sock_path[] = "/dev/socket_seehell";

    memset(&sock_addr, 0, sizeof(sock_addr));
    sock_addr.sun_family = AF_LOCAL;
    strcpy(sock_addr.sun_path, sock_path);	    // adresa = meno soketu (rovnake ako pouziva klient)

    if ((s = socket(PF_LOCAL, SOCK_STREAM, 0)) == -1) {
        // vytvorenie socketu
        perror("socket");
        return ERR_SOCKET;
    }

    printf("Running as client\n");
    if ((connect(s, (struct sockaddr*)&sock_addr, sizeof(sock_addr))) == -1) {
        // pripojenie na server
        perror("socket connect");
        return ERR_SOCKET;
    }

    // toto umoznuje klientovi cakat na vstup z terminalu (stdin) alebo zo soketu
    // co je prave pripravene, to sa obsluzi (nezalezi na poradi v akom to pride)
    FD_ZERO(&rs);
    FD_SET(0, &rs);
    FD_SET(s, &rs);

    while (select(s+1, &rs, NULL, NULL, NULL) > 0) {
        if (FD_ISSET(0, &rs)) { // stdin
            // todo current fgets... implementation goes here instead
            if (fgets(uinput, SHELL_USERINPUT_MAX, stdin) == NULL) return ERR_FGETS;
            rewind(stdin);                        // remove any trailing STDIN
            uinput[strcspn(uinput, "\n")] = '\0'; // remove trailing newline STDOUT
            // pushHistory(history, uinput);         // add to history
            // r = read(STDIN_FILENO, uinput, SHELL_USERINPUT_MAX); 
            write(s, uinput, strlen(uinput));
        }
        if (FD_ISSET(s, &rs)) { // server responded
            r = read(s, uinput, SHELL_USERINPUT_MAX);
            uinput[SHELL_USERINPUT_MAX - 1] = '\0';
            printf("[server response]\n");
            printf("%s\n", uinput);
            printf("[server response end]\n");
        }
        
        // connect() mnoziny meni, takze ich treba znova nastavit
        FD_ZERO(&rs);	
        FD_SET(0, &rs);
        FD_SET(s, &rs);
    }
    perror("select");	// ak server skonci, nemusi ist o chybu

    close(s);
}