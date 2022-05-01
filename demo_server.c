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
    int s, ds, r;
    char uinput[SHELL_USERINPUT_MAX];   // uinputer pre prijatu spravu 
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
    if (unlink(sock_path) == -1) { 
        // ak by tam uz taky bol tak sa vyhodi
        if (errno != ENOENT) { // not found error can be ignored
            perror("socket unlink"); 
            return ERR_SOCKET;
        }
    }		
    if (bind(s, (struct sockaddr*)&sock_addr, sizeof(sock_addr)) == -1) {	
        // zviazat soket s lokalnou adresou
        perror("socket bind");
        return ERR_SOCKET;
    }
    if (listen(s, 5) == -1) { 
        // s je hlavny soket, len pocuva
        // pocuvat, najviac 5 spojeni naraz (v rade)
        perror("socket listen");
        return ERR_SOCKET;
    } 

    // server loop
    printf("Listening...\n");
    while (1 == 1) {
        // prijat jedno spojenie (z max 5 cakajucich)
        if ((ds = accept(s, NULL, NULL)) == -1) {
            perror("data socket");
            return ERR_SOCKET;
        }

        // (sizeof(uinput) - 1) because last char reserved for '\0'
        while ((r = read(ds, &uinput, SHELL_USERINPUT_MAX)) > 0) {
            // guarantee proper ending
            if (r >= SHELL_USERINPUT_MAX)
                uinput[SHELL_USERINPUT_MAX - 1] = '\0';
            else 
                uinput[r] = '\0';

            // request handling
            printf(">> client: %s\n", uinput);

            // server action
            int i;
            for (i=0; i<r; i++) uinput[i] = toupper(uinput[i]);

            // response handling
            printf(">> server: %s\n", uinput);
            if (write(ds, uinput, strlen(uinput)) == -1) {
                perror("data socket write");
                return ERR_SOCKET;
            }
        }
        perror("data socket read");
        close(ds);
    }

    close(s);
}