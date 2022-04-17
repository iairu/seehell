// todo docs
// https://the-linux-channel.the-toffee-project.org/index.php?page=5-tutorials-a-linux-system-call-in-c-without-a-standard-library&lang=en

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define SC_HELP_NOTES
#define SC_UNUSED_FUNC

// syscall interface with assembly (syscall.S)
void* sc_syscall(
    void* syscall_number, // 32 bit 0x80 interrupt based (sys_write => 4, ...)
    // ref: http://faculty.nps.edu/cseagle/assembly/sys_call.html
    void* param1,
    void* param2,
    void* param3,
    void* param4
);

typedef unsigned long int sc_size_t;    // in stdio.h
typedef long int sc_ssize_t;            // in stdio.h
typedef int sc_pid_t;                   // in stdlib.h
typedef int sc_uid_t;                   // in stdlib.h
typedef int sc_gid_t;                   // in stdlib.h
typedef int sc_time_t;                  // in time.h

#ifndef SC_UNUSED_FUNC
// man 2 write
static sc_ssize_t sc_write(int fd, const void* data, size_t nbytes) {
     return (sc_ssize_t) sc_syscall(
         (void*)4,               //stack 1 sys_write
         (void*)(ssize_t)fd,     //stack 2
         (void*)data,            //stack 3
         (void*)nbytes,          //stack 4
         (void*)0                //stack 5 unused
     );
}
#endif

// man 2 time
static sc_time_t sc_time(void) {
    return (sc_time_t) sc_syscall(
        (void*)13,              //stack 1 sys_time
        (void*)0,               //stack 2 can be a location for what is also the return value
        (void*)0,               //stack 3 unused
        (void*)0,               //stack 4 unused
        (void*)0                //stack 5 unused
    );
}
#ifndef SC_HELP_NOTES
// time.h - conversion of given unix time
struct tm {
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
};
struct tm *localtime(const time_t *_Time);
#endif

// man 2 getuid
static sc_uid_t sc_getuid(void) {
    return (sc_uid_t) sc_syscall(
        (void*)24,              //stack 1 sys_getuid
        (void*)0,               //stack 2 unused
        (void*)0,               //stack 3 unused
        (void*)0,               //stack 4 unused
        (void*)0                //stack 5 unused
    );
}

#ifndef SC_UNUSED_FUNC
// man 2 getgid
static sc_uid_t sc_getgid(void) {
    return (sc_uid_t) sc_syscall(
        (void*)47,              //stack 1 sys_getgid
        (void*)0,               //stack 2 unused
        (void*)0,               //stack 3 unused
        (void*)0,               //stack 4 unused
        (void*)0                //stack 5 unused
    );
}
#endif

// man 3 getpwuid
struct passwd {
    char   *pw_name;       // username
    char   *pw_passwd;     // user password
    sc_uid_t   pw_uid;     // user ID
    sc_gid_t   pw_gid;     // group ID
    char   *pw_gecos;      // user information
    char   *pw_dir;        // home directory
    char   *pw_shell;      // shell program
};
struct passwd *getpwuid(sc_uid_t uid); // builtin

// man 2 gethostname
#ifndef SC_HELP_NOTES
// unistd.h
int gethostname(char *name, sc_size_t len); 
#endif

// man 2 chdir
#ifndef SC_HELP_NOTES
// unistd.h
int chdir(const char *path);
#endif
