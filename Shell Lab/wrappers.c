#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <errno.h>
#include "wrappers.h"

//add the missing wrappers to this file (for fork, exec, sigprocmask, etc.)

/*
Fork is a wrapper function for fork(), checks to make sure the pid returned from
fork is not less than zero.
*/
pid_t Fork(void) {
    pid_t pid;
    //printf("aras");
    if((pid = fork()) < 0) {
        //printf("pid error in Fork wrapper");
        unix_error("Fork error");
        //return;
    }
    return pid;
}

/*
Exec is a wrapper function for exec()
*/
void Execve(char *argv, char **a, char **environ) {
    if (execve(argv, a, environ) < 0) {
        printf("%s: Command not found.\n", argv[0]);
        //exit(0);
    }
}

/*
* Sigprocmask is the wrapper function for sigprocmask()
*/
void Sigprocmask(int how, const sigset_t *set,
                 sigset_t *oset) {
    if(sigprocmask(how, set, oset) == -1) {
        unix_error("sigprocmask error");
        //exit(0);
    }
}

/*
* Sigfillset is the wrapper function for sigfillset()
*/
void Sigfillset(sigset_t *set) {
    if(sigfillset(set) == -1) {
        unix_error("sigfillset error");
        //exit(0);
    }
}

/*
* Sigemptyset is the wrapper function for sigemptyset()
*/
void Sigemptyset(sigset_t *set) {
    if(sigemptyset(set) == -1) {
        unix_error("sigemptyset error");
        //exit(0);
    }
}

/*
* Sigaddset() is the wrapper function for sigaddset()
*/
void Sigaddset(sigset_t *set, int signum) {
    if(sigaddset(set, signum) == -1) {
        unix_error("sigaddset error");
        //exit(0);
    }
}

/*
 * unix_error - unix-style error routine
 */
void unix_error(char *msg)
{
    fprintf(stdout, "%s: %s\n", msg, strerror(errno));
    exit(1);
}

/*
 * app_error - application-style error routine
 */
void app_error(char *msg)
{
    fprintf(stdout, "%s\n", msg);
    exit(1);
}

/*
 * Signal - wrapper for the sigaction function
 */
handler_t *Signal(int signum, handler_t *handler) 
{
    struct sigaction action, old_action;

    action.sa_handler = handler;  
    sigemptyset(&action.sa_mask); /* block sigs of type being handled */
    action.sa_flags = SA_RESTART; /* restart syscalls if possible */

    if (sigaction(signum, &action, &old_action) < 0)
	unix_error("Signal error");
    return (old_action.sa_handler);
}


