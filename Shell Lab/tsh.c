/* 
 * tsh - A tiny shell program with job control
 * 
 * <Put your name(s) and login ID(s) here>
 * Nolan Dowdle (dowdlenj)
 * Chase Faine (fainect)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include "jobs.h"      //prototypes for functions that manage the jobs list
#include "wrappers.h"  //prototypes for functions in wrappers.c

/* Misc manifest constants */
#define MAXLINE    1024   /* max line size */
#define MAXARGS     128   /* max args on a command line */

/* Global variables */
extern char **environ;      /* defined in libc */
char prompt[] = "tsh> ";    /* command line prompt (DO NOT CHANGE) */
int verbose = 0;            /* if true, print additional output (-v option) */
char sbuf[MAXLINE];         /* for composing sprintf messages */

/* Here are the prototypes for the functions that you will 
 * implement in this file.
 */
void eval(char *cmdline);
int builtin_cmd(char **argv);
void do_bgfg(char **argv);
void waitfg(pid_t pid);
void sigchld_handler(int sig);
void sigtstp_handler(int sig);
void sigint_handler(int sig);

/* Routines in this file that are already written */
int parseline(const char *cmdline, char **argv); 
void sigquit_handler(int sig);
void usage(void);

/*
 * main - The shell's main routine 
 *        The main is complete. You have nothing to do here.
 */
int main(int argc, char **argv) 
{
    char c;
    char cmdline[MAXLINE];
    int emit_prompt = 1; /* emit prompt (default) */

    /* Redirect stderr to stdout (so that driver will get all output
     * on the pipe connected to stdout) */
    dup2(1, 2);

    /* Parse the command line */
    while ((c = getopt(argc, argv, "hvp")) != EOF) 
    {
        switch (c) 
        {
            case 'h':             /* print help message */
                usage();
                break;
            case 'v':             /* emit additional diagnostic info */
                verbose = 1;
                break;
            case 'p':             /* don't print a prompt */
                emit_prompt = 0;  /* handy for automatic testing */
                break;
            default:
                usage();
        }
    }

    /* Install the signal handlers */

    /* These are the ones you will need to implement */
    Signal(SIGINT,  sigint_handler);   /* ctrl-c */
    Signal(SIGTSTP, sigtstp_handler);  /* ctrl-z */
    Signal(SIGCHLD, sigchld_handler);  /* Terminated or stopped child */

    /* This one provides a clean way to kill the shell */
    Signal(SIGQUIT, sigquit_handler); 

    /* Initialize the job list */
    initjobs(jobs);

    /* Execute the shell's read/eval loop */
    while (1) 
    {

        /* Read command line */
        if (emit_prompt) 
        {
            printf("%s", prompt);
            fflush(stdout);
        }
        if ((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin))
            app_error("fgets error");
        if (feof(stdin)) 
        { /* End of file (ctrl-d) */
            fflush(stdout);
            exit(0);
        }

        /* Evaluate the command line */
        eval(cmdline);
        fflush(stdout);
        fflush(stdout);
    } 

    exit(0); /* control never reaches here */
}

/* Your work involves writing the functions below.  Plus, you'll need
 * to implement wrappers for system calls (Fork, Exec, etc.) Those wrappers
 * will go in wrappers.c.  You'll put prototypes for those in wrappers.h.
 *
 * Do NOT just start implementing the functions below.  You will do trace file
 * by trace file development by following the steps in the lab.
 * Seriously, do not do this any other way. You will be sorry.
 */
  
/* 
 * eval - Evaluate the command line that the user has just typed in
 * 
 * If the user has requested a built-in command (quit, jobs, bg or fg)
 * then execute it immediately. Otherwise, fork a child process and
 * run the job in the context of the child. If the job is running in
 * the foreground, wait for it to terminate and then return.  Note:
 * each child process must have a unique process group ID so that our
 * background children don't receive SIGINT (SIGTSTP) from the kernel
 * when we type ctrl-c (ctrl-z) at the keyboard.  
*/
void eval(char *cmdline) 
{
    char *argv[MAXARGS];
    char buf[MAXLINE];
    int bg;
    pid_t pid;

    strcpy(buf, cmdline);
    bg = parseline(buf, argv);
    if(argv[0] == NULL) {
        return;
    }
    if(!builtin_cmd(argv)) {
        FILE *file;
        if((file = fopen(argv[0],"r"))!=NULL)
        {
            // file exists
            fclose(file);
        }
        else
        {
            printf("%s: Command not found\n", argv[0]);
            return;
            //File not found, no memory leak since 'file' == NULL
            //fclose(file) would cause an error
        }

        sigset_t mask_all, mask_one, prev_one;
        Sigfillset(&mask_all);
        Sigemptyset(&mask_one);
        Sigaddset(&mask_one, SIGCHLD);
        Sigprocmask(SIG_BLOCK, &mask_one, &prev_one);
        if((pid = Fork()) == 0) {
            Sigprocmask(SIG_SETMASK, &prev_one, NULL);
            setpgid(0,0);
            Execve(argv[0], argv, environ);
        }
        if(!bg) {
            
            Sigprocmask(SIG_BLOCK, &mask_all, NULL);
            addjob(jobs, pid, FG, cmdline);
            Sigprocmask(SIG_SETMASK, &prev_one, NULL);
            waitfg(pid);
        } else {
            Sigprocmask(SIG_BLOCK, &mask_all, NULL);
            addjob(jobs, pid, BG, cmdline);
            Sigprocmask(SIG_SETMASK, &prev_one, NULL);
            int jid = pid2jid(pid);
            printf("[%d] (%d) %s", jid, pid, cmdline);
        }
    }
    return;
}

/* 
 * builtin_cmd - If the user has typed a built-in command then execute
 *    it immediately.  
 */
int builtin_cmd(char **argv) 
{
    if (!strcmp(argv[0], "quit")) {
        exit(0);
    }
    if (!strcmp(argv[0], "jobs")) {
        listjobs(jobs);
        return 1;
    }
    if (!strcmp(argv[0], "bg")) {
        do_bgfg(argv);
        return 1;
    }
    if (!strcmp(argv[0], "fg")) {
        do_bgfg(argv);
        return 1;
    }
    return 0;     /* not a builtin command */
}

/* 
 * do_bgfg - Execute the builtin bg and fg commands
 */
void do_bgfg(char **argv) 
{
    job_t * temp;
    int jid;
    pid_t pid;
    sigset_t mask_all, prev_all;
    if(argv[1] == NULL) {
        printf("%s command requires PID or %%jobid argument\n", argv[0]);
        return;
    }

    if (argv[1][0] == '%') {
        if(!isdigit(argv[1][1])) {
            printf("%s: argument must be a PID or %%jobid\n", argv[0]);
            return;
        }
        jid = atoi(&argv[1][1]);
        temp = getjobjid(jobs, jid);
        if(temp == NULL) {
            printf("%%%d: No such job\n",  jid);
            return;
        }
    }
    else {
        if(!isdigit(argv[1][0])) {
            printf("%s: argument must be a PID or %%jobid\n", argv[0]);
            return;
        }
        pid = atoi(&argv[1][0]);
        temp = getjobpid(jobs, pid);
        if(temp == NULL) {
            printf("(%d): No such process\n", pid);
            return;
        }
    }
    if (!strcmp(argv[0], "bg"))
    {
        kill((-(temp->pid)), SIGCONT);
        Sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
        temp->state = BG;
        Sigprocmask(SIG_SETMASK, &prev_all, NULL);
        printf("[%d] (%d) %s", temp->jid, temp->pid, temp->cmdline);
    } else if (!strcmp(argv[0], "fg"))
    {
        if(temp->state = ST) {
            kill((-(temp->pid)), SIGCONT);
        }
        Sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
        temp->state = FG;
        Sigprocmask(SIG_SETMASK, &prev_all, NULL);
        waitfg(fgpid(jobs));
    }
    return;
}

/* 
 * waitfg - Block until process pid is no longer the foreground process
 */
void waitfg(pid_t pid)
{
    pid_t pp = fgpid(jobs);
    while(pid == pp) {
        sleep(1);
        pp = fgpid(jobs);
    }
    return;
}

/*****************
 * Signal handlers
 *****************/

/* 
 * sigchld_handler - The kernel sends a SIGCHLD to the shell whenever
 *     a child job terminates (becomes a zombie), or stops because it
 *     received a SIGSTOP or SIGTSTP signal. The handler reaps all
 *     available zombie children, but doesn't wait for any other
 *     currently running children to terminate.  
 */
void sigchld_handler(int sig) 
{
    int status;
    pid_t x;
    sigset_t mask_all, prev_all;
    while((x = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) {
        //printf("pid is: %d\n", x);
        int jid = pid2jid(x);
        //printf("jid is: %d\n\n", jid);
        if(jid == 0) {
            printf("jid not found for given pid!");
            continue;
        }
        Sigfillset(&mask_all);
        if(WIFEXITED(status)) {
            //block all signals
            Sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
            deletejob(jobs, x); /* Delete the child from the job list */
            Sigprocmask(SIG_SETMASK, &prev_all, NULL);
        } else if (WIFCONTINUED(status)) {

            continue;
        } else if (WIFSIGNALED(status)) {
            int terminationSig = WTERMSIG(status);
            job_t * job = getjobpid(jobs, x);
            int jid = pid2jid(x);
            if(terminationSig == 11) {
                Sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
                deletejob(jobs, x); /* Delete the child from the job list */
                Sigprocmask(SIG_SETMASK, &prev_all, NULL);
                continue;
            }
            printf("Job [%d] (%d) terminated by signal %d\n", jid, x, terminationSig);
            Sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
            deletejob(jobs, x); /* Delete the child from the job list */
            Sigprocmask(SIG_SETMASK, &prev_all, NULL);

        } else if (WIFSTOPPED(status)) {
            int stopSig = WSTOPSIG(status);
            int jid = pid2jid(x);
            job_t * temp;
            temp = getjobjid(jobs, jid);
            Sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
            temp->state = ST;
            Sigprocmask(SIG_SETMASK, &prev_all, NULL);
            printf("Job [%d] (%d) stopped by signal %d\n", jid, x, stopSig);
        }
    }
    return;
}

/* 
 * sigint_handler - The kernel sends a SIGINT to the shell whenver the
 *    user types ctrl-c at the keyboard.  Catch it and send it along
 *    to the foreground job.  
 */
void sigint_handler(int sig) 
{
    pid_t pp = fgpid(jobs);
    if(pp > 0) {
        kill((-(pp)), sig);
    }
    return;
}

/*
 * sigtstp_handler - The kernel sends a SIGTSTP to the shell whenever
 *     the user types ctrl-z at the keyboard. Catch it and suspend the
 *     foreground job by sending it a SIGTSTP.  
 */
void sigtstp_handler(int sig)
{
    pid_t pp = fgpid(jobs);
    if(pp > 0) {
        kill(-pp, SIGTSTP);
    }
    return;
}

/*************************************
 *  The rest of these are complete.
 *************************************/


/*
 * sigquit_handler - The driver program can gracefully terminate the
 *    child shell by sending it a SIGQUIT signal.
 */
void sigquit_handler(int sig)
{
    printf("Terminating after receipt of SIGQUIT signal\n");
    exit(1);
}

/* 
 * parseline - Parse the command line and build the argv array.
 * 
 * Characters enclosed in single quotes are treated as a single
 * argument.  Return true if the user has requested a BG job, false if
 * the user has requested a FG job.  
 */
int parseline(const char *cmdline, char **argv)
{
    static char array[MAXLINE]; /* holds local copy of command line */
    char *buf = array;          /* ptr that traverses command line */
    char *delim;                /* points to first space delimiter */
    int argc;                   /* number of args */
    int bg;                     /* background job? */

    strcpy(buf, cmdline);
    buf[strlen(buf)-1] = ' ';  /* replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) buf++; /* ignore leading spaces */

    /* Build the argv list */
    argc = 0;
    if (*buf == '\'')
    {
        buf++;
        delim = strchr(buf, '\'');
    }
    else
    {
        delim = strchr(buf, ' ');
    }
    while (delim)
    {
        argv[argc++] = buf;
        *delim = '\0';
        buf = delim + 1;
        while (*buf && (*buf == ' ')) buf++; /* ignore spaces */

        if (*buf == '\'')
        {
            buf++;
            delim = strchr(buf, '\'');
        }
        else
        {
            delim = strchr(buf, ' ');
        }
    }
    argv[argc] = NULL;

    if (argc == 0)  return 1; /* ignore blank line */

    /* should the job run in the background? */
    if ((bg = (*argv[argc-1] == '&')) != 0)
    {
        argv[--argc] = NULL;
    }
    return bg;
}


/*
 * usage - print a help message
 */
void usage(void) 
{
    printf("Usage: shell [-hvp]\n");
    printf("   -h   print this message\n");
    //-v enables verbose
    printf("   -v   print additional diagnostic information\n");
    //the tester uses the -p option
    printf("   -p   do not emit a command prompt\n");
    exit(1);
}

