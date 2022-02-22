/* 
 * tsh - A tiny shell program with job control
 * 
 * <Put your student number and login ID here>
 * student number: 2018-10725
 * login ID: stu67
 *   
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

/* Misc manifest constants */
#define MAXLINE    1024   /* max line size */
#define MAXARGS     128   /* max args on a command line */
#define MAXJOBS      16   /* max jobs at any point in time */
#define MAXJID    1<<16   /* max job ID */

/* Job states */
#define UNDEF 0 /* undefined */
#define FG 1    /* running in foreground */
#define BG 2    /* running in background */
#define ST 3    /* stopped */

/* 
 * Jobs states: FG (foreground), BG (background), ST (stopped)
 * Job state transitions and enabling actions:
 *     FG -> ST  : ctrl-z
 *     ST -> FG  : fg command
 *     ST -> BG  : bg commandh
 *     BG -> FG  : fg command
 * At most 1 job can be in the FG state.
 */

/* Global variables */
extern char **environ;      /* defined in libc */
char prompt[] = "tsh> ";    /* command line prompt (DO NOT CHANGE) */
int verbose = 0;            /* if true, print additional output */
int nextjid = 1;            /* next job ID to allocate */
char sbuf[MAXLINE];         /* for composing sprintf messages */

struct job_t {              /* The job struct */
    pid_t pid;              /* job PID */
    int jid;                /* job ID [1, 2, ...] */
    int state;              /* UNDEF, BG, FG, or ST */
    char cmdline[MAXLINE];  /* command line */
};
struct job_t jobs[MAXJOBS]; /* The job list */
/* End global variables */


/* Function prototypes */

/* Here are the functions that you will implement */
void eval(char *cmdline);
int builtin_cmd(char **argv);
void do_bgfg(char **argv);
void waitfg(pid_t pid);

void sigchld_handler(int sig);
void sigtstp_handler(int sig);
void sigint_handler(int sig);

/* Here are helper routines that we've provided for you */
int parseline(const char *cmdline, char **argv); 
void sigquit_handler(int sig);

void clearjob(struct job_t *job);
void initjobs(struct job_t *jobs);
int maxjid(struct job_t *jobs); 
int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline);
int deletejob(struct job_t *jobs, pid_t pid); 
pid_t fgpid(struct job_t *jobs);
struct job_t *getjobpid(struct job_t *jobs, pid_t pid);
struct job_t *getjobjid(struct job_t *jobs, int jid); 
int pid2jid(pid_t pid); 
void listjobs(struct job_t *jobs);

void usage(void);
void unix_error(char *msg);
void app_error(char *msg);
typedef void handler_t(int);
handler_t *Signal(int signum, handler_t *handler);

/*
 * main - The shell's main routine 
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
    while ((c = getopt(argc, argv, "hvp")) != EOF) {
        switch (c) {
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
    while (1) {

	/* Read command line */
	if (emit_prompt) {
	    printf("%s", prompt);
	    fflush(stdout);
	}
	if ((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin))
	    app_error("fgets error");
	if (feof(stdin)) { /* End of file (ctrl-d) */
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
    char** argv;
    
    argv = (char**)calloc(MAXARGS, sizeof(char*));    
    for(int i =0; i<MAXARGS; i++){
        argv[i] = (char*)calloc(MAXLINE, sizeof(char));
    }

    int bg=0; // 1 if background job
    pid_t pid;

    bg = parseline(cmdline, argv);


    //empty input(just space + enter)
    if(argv[0]==NULL) return;

    //block signals
    sigset_t set;

    if(!builtin_cmd(argv)){
        if(sigemptyset(&set)<0) unix_error("Fail to empty signal set");
        if(sigaddset(&set, SIGCHLD)<0) unix_error("Fail to add SIGCHLD to signal set");
        if(sigprocmask(SIG_BLOCK, &set, NULL)<0) unix_error("Fail to block SIGCHLD signal");

        if((pid=fork())==0){ /* child process runs user job*/ 
            //unblock signals
             if(sigprocmask(SIG_UNBLOCK, &set, NULL)<0) unix_error("Fail to Unblock SIGCHILD signal");

            //change pgid
            if(setpgid(0, 0)<0) unix_error("Fail to get new process group ID");

            //Load & Run new program
            if(execve(argv[0], argv, environ)<0){
                printf("%s: Command not found\n", argv[0]);
                exit(0);
            }
        }

    /* Parent process */
       
        //Add job
        addjob(jobs, pid, bg?BG:FG, cmdline);

        //Unblock signals
        if(sigprocmask(SIG_UNBLOCK, &set, NULL)<0) unix_error("Fail to Unblock SIGCHILD signal");

        if(!bg){  /* parent waits for fg job to terminate */
            waitfg(pid);
        }
        //print log message for bg job
        else printf("[%d] (%d) %s", pid2jid(pid), pid, cmdline);
    }

    free(argv);
    return;
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
    while (*buf && (*buf == ' ')) /* ignore leading spaces */
	buf++;

    /* Build the argv list */
    argc = 0;
    if (*buf == '\'') {
	buf++;
	delim = strchr(buf, '\'');
    }
    else {
	delim = strchr(buf, ' ');
    }

    while (delim) {
	argv[argc++] = buf;
	*delim = '\0';
	buf = delim + 1;
	while (*buf && (*buf == ' ')) /* ignore spaces */
	       buf++;

	if (*buf == '\'') {
	    buf++;
	    delim = strchr(buf, '\'');
	}
	else {
	    delim = strchr(buf, ' ');
	}
    }
    argv[argc] = NULL;
    
    if (argc == 0)  /* ignore blank line */
	return 1;

    /* should the job run in the background? */
    if ((bg = (*argv[argc-1] == '&')) != 0) {
	argv[--argc] = NULL;
    }
    return bg;
}

/* 
 * builtin_cmd - If the user has typed a built-in command then execute
 *    it immediately.  
 */
int builtin_cmd(char **argv) 
{

    if(strcmp(argv[0], "quit")==0){
        exit(0);
        return 1;
    }
    else if(strcmp(argv[0], "jobs")==0){
        listjobs(jobs);
        return 1;
    }
    else if(strcmp(argv[0], "fg")==0||strcmp(argv[0], "bg")==0){
        do_bgfg(argv);
        return 1;
    }
    else return 0;     /* not a builtin command */
}

/* 
 * do_bgfg - Execute the builtin bg and fg commands
 */
void do_bgfg(char **argv) 
{
    int cmd =BG;
    int pid; //pid of given <job> argument
    int jid; //jid of given <job> argument
    struct job_t* job; // job given by <job> argument

    if(strcmp(argv[0], "bg")==0) cmd=BG;
    else if(strcmp(argv[0], "fg")==0) cmd =FG;

    //parsing <job> argument & error handling for wrong inputs
    //no <job> argument
    if(argv[1]==NULL){
        printf("%s command requires PID or %%jobid argument\n", argv[0]);
        return;
    }

    //if <job> argument is not jobid or pid -> just made  to work as tshref(if input such as %a or 123abc cmes, tshref prints No such process/job message)
    //tshref works properly when the <job> argument start with number, even if the whole argument itself is wrong, so I made identical to it.
    if(argv[1][0]!='%'&&!(argv[1][0]>='1'&&argv[1][0]<='9')){
        printf("%s: argument must be a PID or %%jobid\n", argv[0]);
        return;
    }

    if(argv[1][0]=='%'){
        jid = atoi(strtok(argv[1], "%"));
        job = getjobjid(jobs, jid);
        if(job==NULL){
            printf("%s: No such job\n", argv[1]);
            return;
        }
        pid = job->pid;
    }
    else{
        pid = atoi(argv[1]);
        job = getjobpid(jobs, pid);
        if(job==NULL){
            printf("(%d): No such process\n", pid);
            return;
        }
    }

    int pgid;
    if((pgid = getpgid(pid))<0) unix_error("Fail to get process group ID for given <job> argument") ;
    if(kill((-1)*pgid, SIGCONT)<0) unix_error("Fail to send SIGCONT signal");
    if(cmd==BG){ //runs in background
        job->state = BG;
        printf("[%d] (%d) %s", job->jid, pid, job->cmdline);
    }
    else if(cmd==FG){ //runs in foreground
        job->state = FG;  

        //wait until foreground process terminates...
        waitfg(job->pid);        
    }

    return;
}


/* 
 * waitfg - Block until process pid is no longer the foreground process
 */
void waitfg(pid_t pid)
{
    
    struct job_t *job;

    //wait until process pid is no longer foreground process
    //i.e. wait until the process terminate/stops and the job list is updated by sigchld_handler
    while((job = getjobpid(jobs, pid))!=NULL){
        if(job->state!=FG) break;        
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
    pid_t pid;
    
    //loop: when there exist child process that should be reaped
    //-1: any child process / WNOHANG: does not wait for currently running children to terminate)  --> to reap any existing zombie child   
    //WUNTRACED: to get info of stopped child --> to deal with children stopped by SIGSTP 
    //WCONTINUED: to trace child continued by fg/bg command(SIGCONT signal)

    while((pid = waitpid(-1, &status, WNOHANG|WUNTRACED|WCONTINUED))>0){ 
        
        struct job_t *job = getjobpid(jobs, pid);

        //update joblist
        if(WIFEXITED(status)!=0) deletejob(jobs, pid);   

        //log message if job terminated because it recieved a signal that it didn't catch + update joblist
        if(WIFSTOPPED(status)){
            if(job!=NULL&&job->state!=ST) printf("Job [%d] (%d) stopped by signal %d\n", job->jid, job->pid, WSTOPSIG(status));

            //update joblist
            if(job!=NULL&&job->state!=ST) job->state = ST;
        }
        if(WIFSIGNALED(status)){
            if(job!=NULL) printf("Job [%d] (%d) terminated by signal %d\n", job->jid, job->pid, WTERMSIG(status));
            deletejob(jobs, pid);   
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
   
    //to check if fg job exists
    int is_fg = 0; 
    int i;

     //find out current fgjob from the job list
    for(i =0; i<MAXJOBS; i++){
        if(jobs[i].state==FG){
            is_fg = 1;
            break;
        }
    }
    
    //if there is no fg job, do nothing and return
    if(!is_fg) return; 
    else{
        //to send sigint to all descendents of current fg job, get pgid
        int pgid;
        
        if((pgid=getpgid(jobs[i].pid))<0) unix_error("Fail to get process group ID of foreground job");

        //send sigint to current fg job and its descendents
        if(kill((-1)*pgid, SIGINT)<0) unix_error("Fail to send SIGINT signal");

        return;
    }
}

/*
 * sigtstp_handler - The kernel sends a SIGTSTP to the shell whenever
 *     the user types ctrl-z at the keyboard. Catch it and suspend the
 *     foreground job by sending it a SIGTSTP.  
 */
void sigtstp_handler(int sig) 
{
    //to check if fg job exists
    int is_fg = 0; 
    
    //find out current fgjob from the job list
    int i=0;
    for(i =0; i<MAXJOBS; i++){
        if(jobs[i].state==FG){
            is_fg = 1;
            break;
        }
    }
    
    //if there is no fg job, do nothing and return
    if(!is_fg) return; 
    else{
        //to send sigint to all descendents of current fg job, get pgid
        int pgid;
        
        if((pgid=getpgid(jobs[i].pid))<0) unix_error("Fail to get process group ID of foreground job");

        //send sigint to current fg job and its descendents
        if(kill((-1)*pgid, SIGTSTP)<0) unix_error("Fail to send SIGTSTP signal");
       

        return;
    }
}

/*********************
 * End signal handlers
 *********************/

/***********************************************
 * Helper routines that manipulate the job list
 **********************************************/

/* clearjob - Clear the entries in a job struct */
void clearjob(struct job_t *job) {
    job->pid = 0;
    job->jid = 0;
    job->state = UNDEF;
    job->cmdline[0] = '\0';
}

/* initjobs - Initialize the job list */
void initjobs(struct job_t *jobs) {
    int i;

    for (i = 0; i < MAXJOBS; i++)
	clearjob(&jobs[i]);
}

/* maxjid - Returns largest allocated job ID */
int maxjid(struct job_t *jobs) 
{
    int i, max=0;

    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].jid > max)
	    max = jobs[i].jid;
    return max;
}

/* addjob - Add a job to the job list */
int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline) 
{
    int i;
    
    if (pid < 1)
	return 0;

    for (i = 0; i < MAXJOBS; i++) {
	if (jobs[i].pid == 0) {
	    jobs[i].pid = pid;
	    jobs[i].state = state;
	    jobs[i].jid = nextjid++;
	    if (nextjid > MAXJOBS)
		nextjid = 1;
	    strcpy(jobs[i].cmdline, cmdline);
  	    if(verbose){
	        printf("Added job [%d] %d %s\n", jobs[i].jid, jobs[i].pid, jobs[i].cmdline);
            }
            return 1;
	}
    }
    printf("Tried to create too many jobs\n");
    return 0;
}

/* deletejob - Delete a job whose PID=pid from the job list */
int deletejob(struct job_t *jobs, pid_t pid) 
{
    int i;

    if (pid < 1)
	return 0;

    for (i = 0; i < MAXJOBS; i++) {
	if (jobs[i].pid == pid) {
	    clearjob(&jobs[i]);
	    nextjid = maxjid(jobs)+1;
	    return 1;
	}
    }
    return 0;
}

/* fgpid - Return PID of current foreground job, 0 if no such job */
pid_t fgpid(struct job_t *jobs) {
    int i;

    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].state == FG)
	    return jobs[i].pid;
    return 0;
}

/* getjobpid  - Find a job (by PID) on the job list */
struct job_t *getjobpid(struct job_t *jobs, pid_t pid) {
    int i;

    if (pid < 1)
	return NULL;
    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].pid == pid)
	    return &jobs[i];
    return NULL;
}

/* getjobjid  - Find a job (by JID) on the job list */
struct job_t *getjobjid(struct job_t *jobs, int jid) 
{
    int i;

    if (jid < 1)
	return NULL;
    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].jid == jid)
	    return &jobs[i];
    return NULL;
}

/* pid2jid - Map process ID to job ID */
int pid2jid(pid_t pid) 
{
    int i;

    if (pid < 1)
	return 0;
    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].pid == pid) {
            return jobs[i].jid;
        }
    return 0;
}

/* listjobs - Print the job list */
void listjobs(struct job_t *jobs) 
{
    int i;
    
    for (i = 0; i < MAXJOBS; i++) {
	if (jobs[i].pid != 0) {
	    printf("[%d] (%d) ", jobs[i].jid, jobs[i].pid);
	    switch (jobs[i].state) {
		case BG: 
		    printf("Running ");
		    break;
		case FG: 
		    printf("Foreground ");
		    break;
		case ST: 
		    printf("Stopped ");
		    break;
	    default:
		    printf("listjobs: Internal error: job[%d].state=%d ", 
			   i, jobs[i].state);
	    }
	    printf("%s", jobs[i].cmdline);
	}
    }
}
/******************************
 * end job list helper routines
 ******************************/


/***********************
 * Other helper routines
 ***********************/

/*
 * usage - print a help message
 */
void usage(void) 
{
    printf("Usage: shell [-hvp]\n");
    printf("   -h   print this message\n");
    printf("   -v   print additional diagnostic information\n");
    printf("   -p   do not emit a command prompt\n");
    exit(1);
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

/*
 * sigquit_handler - The driver program can gracefully terminate the
 *    child shell by sending it a SIGQUIT signal.
 */
void sigquit_handler(int sig) 
{
    printf("Terminating after receipt of SIGQUIT signal\n");
    exit(1);
}



