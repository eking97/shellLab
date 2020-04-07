// 
// tsh - A tiny shell program with job control
// 
// <Put your name and login ID here>
// Emma King - emki0195 - eking97
//

using namespace std;

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <string>

#include "globals.h"
#include "jobs.h"
#include "helper-routines.h"

//
// Needed global variable definitions
//

static char prompt[] = "tsh> ";
int verbose = 0;

//
// You need to implement the functions eval, builtin_cmd, do_bgfg,
// waitfg, sigchld_handler, sigstp_handler, sigint_handler
//
// 1.) Amit said to start with builtin_cmd cause it's the easiest to begin
// 2.) do eval next so we can check the cmds input (ex. quit)
// - while working on eval also implement waitfg and signal handlers

// The code below provides the "prototypes" for those functions
// so that earlier code can refer to them. You need to fill in the
// function bodies below.
// 

void eval(char *cmdline);
int builtin_cmd(char **argv);
void do_bgfg(char **argv);
void waitfg(pid_t pid);

void sigchld_handler(int sig);
void sigtstp_handler(int sig);
void sigint_handler(int sig);

//
// main - The shell's main routine 
//
int main(int argc, char *argv[]) 
{
  int emit_prompt = 1; // emit prompt (default)

  //
  // Redirect stderr to stdout (so that driver will get all output
  // on the pipe connected to stdout)
  //
  dup2(1, 2);

  /* Parse the command line */
  char c;
  while ((c = getopt(argc, argv, "hvp")) != EOF) {
    switch (c) {
    case 'h':             // print help message
      usage();
      break;
    case 'v':             // emit additional diagnostic info
      verbose = 1;
      break;
    case 'p':             // don't print a prompt
      emit_prompt = 0;  // handy for automatic testing
      break;
    default:
      usage();
    }
  }

  //
  // Install the signal handlers
  //

  //
  // These are the ones you will need to implement
  //
  Signal(SIGINT,  sigint_handler);   // ctrl-c
  Signal(SIGTSTP, sigtstp_handler);  // ctrl-z
  Signal(SIGCHLD, sigchld_handler);  // Terminated or stopped child

  //
  // This one provides a clean way to kill the shell
  //
  Signal(SIGQUIT, sigquit_handler); 

  //
  // Initialize the job list
  //
  initjobs(jobs);

  //
  // Execute the shell's read/eval loop
  //
  for(;;) {
    //
    // Read command line
    //
    if (emit_prompt) {
      printf("%s", prompt);
      fflush(stdout);
    }

    char cmdline[MAXLINE];

    if ((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin)) {
      app_error("fgets error");
    }
    //
    // End of file? (did user type ctrl-d?)
    //
    if (feof(stdin)) {
      fflush(stdout);
      exit(0);
    }

    //
    // Evaluate command line
    //
    eval(cmdline); //calling the eval function - important to get it done!
    fflush(stdout);
    fflush(stdout);
  } 

  exit(0); //control never reaches here
}
  
/////////////////////////////////////////////////////////////////////////////
//
// eval - Evaluate the command line that the user has just typed in
// 
// If the user has requested a built-in command (quit, jobs, bg or fg)
// then execute it immediately. Otherwise, fork a child process and
// run the job in the context of the child. If the job is running in
// the foreground, wait for it to terminate and then return.  Note:
// each child process must have a unique process group ID so that our
// background children don't receive SIGINT (SIGTSTP) from the kernel
// when we type ctrl-c (ctrl-z) at the keyboard.
//
//about 70 lines
void eval(char *cmdline) 
{
  /* Parse command line */
  //
  // The 'argv' vector is filled in by the parseline
  // routine below. It provides the arguments needed
  // for the execve() routine, which you'll need to
  // use below to launch a process.
  //
    //we will do different things depending on if it's an actual command or if it's a built_in command
    // if it's not built_in, we should fork an exact child process
    // if it's built_in then we will try to carry out command without a child process
    
    
  //
  // The 'bg' variable is TRUE if the job should run
  // in background mode or FALSE if it should run in FG
  //
  pid_t pid; //comes from jobs.h - job process ID
  char *argv[MAXARGS]; //create the array of pointers that we will pass to parseline - MAXARGS defined above
  int bg = parseline(cmdline, argv); 
  struct job_t *job;
  
     if (!builtin_cmd(argv)) //if it is not a built in command
    {
        //forking and execing a child process
         if ((pid = fork()) == 0) //equals zero so we are now within the child process
         {
             setpgid(0, 0); //The setpgid function changes the process group of process pid to pgid - in write-up
             if (execvp(argv[0], argv) < 0) //exec fails
             {
                 printf("%s: Command not found\n", argv[0]); //exec shouldn't return unless something went wrong
                 exit(0); //keeps child from forking itself into another child process when an unknown command is input into terminal
             }
//execvp(argv[0], argv); //exec variant, first thing we give to it is the path to the command -rest of cmd passed in subsequently as an  argument vector - p means it's gonna look things up in its path
             //printf("forked child");
         }
         addjob(jobs, pid, bg ? BG : FG, cmdline); //if bg is true do background, otherwise do foreground
         if (!bg) //if in the parent process - foreground
        {
//              printf("foreground");
            //parent process wait for the child process and also reap it at the same time
            //wait(NULL); //don't care about the return value of the child process
            waitfg(pid);
        }
        if (bg)
        {
//             printf("background");
            //it is a background job
            //print a status message like tshref
            job = getjobpid(jobs, pid); //call from jobs.h
            printf("[%d] (%d) %s", job->jid, job->pid, cmdline); //could use given job functions but this is easier
        }
    }
  if (argv[0] == NULL)  
  {
    return;   /* ignore empty lines */
  }
  return;
}


/////////////////////////////////////////////////////////////////////////////
//
// builtin_cmd - If the user has typed a built-in command then execute
// it immediately. The command name would be in argv[0] and
// is a C string. We've cast this to a C++ string type to simplify
// string comparisons; however, the do_bgfg routine will need 
// to use the argv array as well to look for a job number.
//
// Recognizes and interprets the built-in commands: quit, fg, bg, and jobs. [25
// lines]

int builtin_cmd(char **argv) 
{
  string cmd(argv[0]);
  
    //user inputs simple command now we need to write up what will happen in shell
  if (cmd == "quit") //the command to quit shell - for quit to work we need to implement eval
  {
     exit(0);  //indicates successful program termination - exit(1) is unsuccessful
  }
  if (cmd == "fg" || cmd == "bg") //have to call other function
  {
      do_bgfg(argv);
      return 1; //want a return statement so after command is run we exit out of shell
  }
  if (cmd == "jobs")
  {
      listjobs(jobs);
      return 1;
  }
  return 0;     /* not a builtin command */
}

/////////////////////////////////////////////////////////////////////////////
//
// do_bgfg - Execute the builtin bg and fg commands
//start this for trace09
void do_bgfg(char **argv) 
{
  struct job_t *jobp=NULL;
    
  /* Ignore command if no argument */
  if (argv[1] == NULL) {
    printf("%s command requires PID or %%jobid argument\n", argv[0]);
    return;
  }
    
  /* Parse the required PID or %JID arg */
  if (isdigit(argv[1][0])) {
    pid_t pid = atoi(argv[1]);
    if (!(jobp = getjobpid(jobs, pid))) {
      printf("(%d): No such process\n", pid);
      return;
    }
  }
  else if (argv[1][0] == '%') {
    int jid = atoi(&argv[1][1]);
    if (!(jobp = getjobjid(jobs, jid))) {
      printf("%s: No such job\n", argv[1]);
      return;
    }
  }	    
  else {
    printf("%s: argument must be a PID or %%jobid\n", argv[0]);
    return;
  }

  //
  // You need to complete rest. At this point,
  // the variable 'jobp' is the job pointer
  // for the job ID specified as an argument.
  //
  // Your actions will depend on the specified command
  // so we've converted argv[0] to a string (cmd) for
  // your benefit.
  //
  string cmd(argv[0]);
  //this is if the user manually inputs "bg" or "fg"
    if (cmd == "bg") //want to run background process
    {
        jobp->state = BG; //have to change the job's state
        printf("[%d] (%d) %s", jobp->jid, jobp->pid, jobp->cmdline); //print same message as eval function if (bg)
        //jid, pid, and cmdline are all variables that can be accessed in a job_t struct
    }
    if (cmd == "fg")//will utilize the waitfg() b/c running in the foreground
    {
        //The fg <job> built-in command restarts <job> by sending it a SIGCONT signal, and then runs it in the foreground
        kill(-(jobp->pid), SIGCONT); //The process stops until restarted by a SIGCONT signal
        jobp->state = FG;
        waitfg(jobp->pid); //wait for job to finish, takes a pid
    }

  return;
}

/////////////////////////////////////////////////////////////////////////////
//
// waitfg - Block until process pid is no longer the foreground process
// In waitfg, use a busy loop around the sleep function.
void waitfg(pid_t pid)
{
//     struct job_t *job = getjobpid(jobs, pid);
//     while (job->state == FG) //if jobs state is equal to its foreground state
//     {
//         sleep(1);
//     } // alternate way to do this
    while(pid == fgpid(jobs)) //waits until the current job is not longer the foreground state
    {
        sleep(1);
    }
  return;
}

/////////////////////////////////////////////////////////////////////////////
//
// Signal handlers
//


/////////////////////////////////////////////////////////////////////////////
//
// sigchld_handler - The kernel sends a SIGCHLD to the shell whenever
//     a child job terminates (becomes a zombie), or stops because it
//     received a SIGSTOP or SIGTSTP signal. The handler reaps all
//     available zombie children, but doesn't wait for any other
//     currently running children to terminate.  
//
//In sigchld handler, use exactly one call to waitpid.
//The WUNTRACED and WNOHANG options to waitpid will also be useful.
// 80 lines
void sigchld_handler(int sig) 
{
    pid_t pid;
    int status; //staus of the job
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0)
    {
        if (WIFSIGNALED(status)) //originally had this in sigint_handler, but was getting different signal values so moved here
            //WIFSIGNALED(status): Returns true if the child process terminated because of a signal that was not caught - pg 725
         { //this is for trace06 and trace07
              //printf("CHILD HANDLER - different signal");
              printf("Job [%d] (%d) terminated by signal %d\n", pid2jid(pid), pid, WTERMSIG(status));
              deletejob(jobs,pid);
            return;
         }
        
        if (WIFSTOPPED(status))
        //WIFSTOPPED(status): Returns true if the child that caused the return is currently stopped
        {
            //printf("CHILD HANDLER - currently stopped");
            getjobpid(jobs, pid)->state = ST;
            printf("Job [%d] (%d) stopped by signal %d\n", pid2jid(pid), pid, WSTOPSIG(status));
            //if deletejob() is placed here doesn't pass trace08
            return;
        }
        if (WIFEXITED(status)) 
        {   
            //printf("CHILD HANDLER - normal termination");
            deletejob(jobs, pid);
            return;
        }
        //deletejob(jobs, pid); //reaping the child
    }
    
  return;
}

/////////////////////////////////////////////////////////////////////////////
//
// sigint_handler - The kernel sends a SIGINT to the shell whenver the
//    user types ctrl-c at the keyboard.  Catch it and send it along
//    to the foreground job.  
// 15 lines
void sigint_handler(int sig) 
{
    //When you implement your signal handlers, be sure to send SIGINT and SIGTSTP signals to the entire foreground process group, using ”-pid” instead     //of ”pid” in the argument to the kill function
    //gonna have to use the kill function - pg 739-740
    pid_t pid;
    pid = fgpid(jobs); //gets the pid of the job running in the foreground
    //printf("HANDLER");
     //If pid is zero, the PID of the current process is used
    if (pid != 0)
    {
       kill(-(pid), SIGINT); //kill entire foreground process group
    }
  return;
}

/////////////////////////////////////////////////////////////////////////////
//
// sigtstp_handler - The kernel sends a SIGTSTP to the shell whenever
//     the user types ctrl-z at the keyboard. Catch it and suspend the
//     foreground job by sending it a SIGTSTP.  
// 15 lines
void sigtstp_handler(int sig) 
{
    //typing crtl-z sends a SIGTSTP signal to the shell, which catches it and sends a SIGTSTP signal to every process in the foreground process group.     //In the default case, the result is to stop (suspend) the foreground job - pg 741
    //stop signal from terminal 
    
    pid_t pid;
    pid = fgpid(jobs);
    
    if (pid != 0)
    {
        kill(-(pid), SIGTSTP); //kill entire foreground process group
    }
  return;
}

/*********************
 * End signal handlers
 *********************/




