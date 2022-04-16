// todo docs
// https://the-linux-channel.the-toffee-project.org/index.php?page=5-tutorials-a-linux-system-call-in-c-without-a-standard-library&lang=en

#include <stdio.h>
#include <stdlib.h>

// syscall interface with assembly (syscall.S)
void* syscall(
    void* syscall_number, // 32 bit 0x80 interrupt based (sys_write => 4, ...)
    // ref: http://faculty.nps.edu/cseagle/assembly/sys_call.html
    void* param1,
    void* param2,
    void* param3,
    void* param4
);

typedef unsigned long int sc_size_t;    // already in stdio.h
typedef long int sc_ssize_t;            // already in stdio.h

// following ids have size of 4 per https://stackoverflow.com/a/1922892
// or use #include <sys/types.h>
typedef int sc_pid_t;                   // already in stdlib.h
typedef int sc_uid_t;                   // already in stdlib.h
typedef int sc_gid_t;                   // already in stdlib.h

// sc_ prefix for my implementations
// no prefix for library functions

// man 2 write
static sc_ssize_t sc_write(int fd, const void* data, size_t nbytes) {
     return (sc_ssize_t) syscall(
         (void*)4,               //stack 1 sys_write
         (void*)(ssize_t)fd,     //stack 2
         (void*)data,            //stack 3
         (void*)nbytes,          //stack 4
         (void*)0                //stack 5 unused
     );
 }

// man 2 getuid
static sc_uid_t sc_getuid(void) {
    return (sc_uid_t) syscall(
        (void*)24,              //stack 1 sys_getuid
        (void*)0,               //stack 2 unused
        (void*)0,               //stack 3 unused
        (void*)0,               //stack 4 unused
        (void*)0                //stack 5 unused
    );
}
// // man 2 getgid
// static sc_uid_t sc_getgid(void) {
//     return (sc_uid_t) syscall(
//         (void*)47,              //stack 1 sys_getgid
//         (void*)0,               //stack 2 unused
//         (void*)0,               //stack 3 unused
//         (void*)0,               //stack 4 unused
//         (void*)0                //stack 5 unused
//     );
// }

// man 3 getpwuid
struct passwd {
    char   *pw_name;       /* username */
    char   *pw_passwd;     /* user password */
    sc_uid_t   pw_uid;        /* user ID */
    sc_gid_t   pw_gid;        /* group ID */
    char   *pw_gecos;      /* user information */
    char   *pw_dir;        /* home directory */
    char   *pw_shell;      /* shell program */
};
struct passwd *getpwuid(sc_uid_t uid); // todo - library function for now (no need to free https://stackoverflow.com/a/160306)

// man 2 gethostname
int gethostname(char *name, sc_size_t len); // todo - library function for now (no need to free)