pid_t Fork(void);
void Execve(char *argv, char **a, char **environ);
void Sigprocmask(int how, const sigset_t *set, sigset_t *oset);
void Sigfillset(sigset_t *set);
void Sigemptyset(sigset_t *set);
void Sigaddset(sigset_t *set, int signum);
void unix_error(char *msg);
void app_error(char *msg);
typedef void handler_t(int);

handler_t *Signal(int signum, handler_t *handler);  //sigaction function wrapper
//You'll need to add prototypes for the other wrappers you write
