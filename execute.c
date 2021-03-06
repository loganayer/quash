/**
 * @file execute.c
 *
 * @brief Implements interface functions between Quash and the environment and
 * functions that interpret an execute commands.
 *
 * @note As you add things to this file you may want to change the method signature
 */

#include "execute.h"

#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <sys/wait.h>
#include "quash.h"
#include <fcntl.h>

// Remove this and all expansion calls to it
/**
 * @brief Note calls to any function that requires implementation
 */
#define IMPLEMENT_ME()                                                  \
	fprintf(stderr, "IMPLEMENT ME: %s(line %d): %s()\n", __FILE__, __LINE__, __FUNCTION__)
#define READ_END 0
#define WRITE_END 1
static int enviroment_pipes[2][2];
static int prev_pipe = -1;
static int next_pipe = 0;
//variable to hold what job we are on, need to increment it as we add jobs
int current_jobid = 0;

//Create our PID queue for a specific job
IMPLEMENT_DEQUE_STRUCT(process_queue, int);
IMPLEMENT_DEQUE(process_queue, int);

//job struct to hold job info and underlying processes
typedef struct job_t{
	int job_id;
	char *cmd;
	static process_queue *pid_queue;;
} job_t;

//create out JOB queue structure to hold background jobs
IMPLEMENT_DEQUE_STRUCT(job_queue, job_t);
IMPLEMENT_DEQUE(job_queue, job_t);

//instantiate static pointer to background jobs queue
static job_queue *background_job_queue;

/***************************************************************************
 * Interface Functions
 ***************************************************************************/


// Return a string containing the current working directory.
char* get_current_directory(bool* should_free) {
	// Change this to true if necessary
	*should_free = true;

	//system call to get the current working directory
	return getcwd( NULL, PATH_MAX + 1 );
}

// Returns the value of an environment variable env_var
const char* lookup_env(const char* env_var) {
	//system call to get the environment variable env_var
	return getenv(env_var);
}

// Check the status of background jobs
void check_jobs_bg_status() {
	// TODO: Check on the statuses of all processes belonging to all background
	// jobs. This function should remove jobs from the jobs queue once all
	// processes belonging to a job have completed.
	IMPLEMENT_ME();
	
	//Look at all jobs in the job queue and check the status of the PIDs in there,
	//If a PID has reported it is finished, pop it from the PID queue
	//If the PID queue is empty, pop that JOB from the JOB queue

	// TODO: Once jobs are implemented, uncomment and fill the following line
	// print_job_bg_complete(job_id, pid, cmd);
}

// Prints the job id number, the process id of the first process belonging to
// the Job, and the command string associated with this job
void print_job(int job_id, pid_t pid, const char* cmd) {
	printf("[%d]\t%8d\t%s\n", job_id, pid, cmd);
	fflush(stdout);
}

// Prints a start up message for background processes
void print_job_bg_start(int job_id, pid_t pid, const char* cmd) {
	printf("Background job started: ");
	print_job(job_id, pid, cmd);
}

// Prints a completion message followed by the print job
void print_job_bg_complete(int job_id, pid_t pid, const char* cmd) {
	printf("Completed: \t");
	print_job(job_id, pid, cmd);
}

/***************************************************************************
 * Functions to process commands
 ***************************************************************************/
// Run a program reachable by the path environment variable, relative path, or
// absolute path
void run_generic(GenericCommand cmd) {
	// Execute a program with a list of arguments. The `args` array is a NULL
	// terminated (last string is always NULL) list of strings. The first element
	// in the array is the executable
	char* exec = cmd.args[0];
	char** args = cmd.args;

	//Overlay calling process with new program
	execvp(exec, args);

	//shouldn't return
	perror("ERROR: Failed to execute program");
}

// Print strings
void run_echo(EchoCommand cmd) {
	// Print an array of strings. The args array is a NULL terminated (last
	// string is always NULL) list of strings.
	char** str = cmd.args;

	//Iterate through array printing each string
	while(*str)
	{
		printf( "%s ", *str++ );
	}
	printf("\n");

	// Flush the buffer before returning
	fflush(stdout);
}

// Sets an environment variable
void run_export(ExportCommand cmd) {
  // Write an environment variable
  const char* env_var = cmd.env_var;
  const char* val = cmd.val;

  if (setenv(env_var, val, 1) == -1)
  {
	  perror("ERROR: ");
	  return;
  }
}

// Changes the current working directory
void run_cd(CDCommand cmd) {
	// Get the directory name
	const char* dir = cmd.dir;

	// Check if the directory is valid
	if (dir == NULL) {
		perror("ERROR: Failed to resolve path");
		return;
	}

	//Get the old directory name
	const char* old_dir = lookup_env("PWD");

	// Change directory
	if (chdir(dir) == -1)
	{
		perror("ERROR: ");
		return;
	}

	// Update the PWD environment variable to be the new current working
	// directory and optionally update OLD_PWD environment variable to be the old
	// working directory.
	if (setenv("PWD", dir, 1) == -1)
	{
		perror("ERROR: ");
		return;
	}
	if (setenv("OLD_PWD", old_dir, 1) == -1)
	{
		perror("ERROR: ");
		return;
	}
}

// Sends a signal to all processes contained in a job
void run_kill(KillCommand cmd) {
	int signal = cmd.sig;
	int job_id = cmd.job;

	// TODO: Remove warning silencers
	(void) signal; // Silence unused variable warning
	(void) job_id; // Silence unused variable warning

	// TODO: Kill all processes associated with a background job
	IMPLEMENT_ME();
	
	//Go to the job queue and find the item with ID = cmd.job then send the cmd.sig to all those PID in that pid queue
	//and pop those til the PID queue is empty
	//then pop the job out of the queue
}


// Prints the current working directory to stdout
void run_pwd() {
	//create a buffer to hold our CWD
	char buffer[PATH_MAX+1];

	//system call to get CWD
	char* pwd = getcwd(buffer, PATH_MAX+1);

	//Simply print
	printf("%s\n", pwd);

	// Flush the buffer before returning
	fflush(stdout);
	return;
}

// Prints all background jobs currently in the job list to stdout
void run_jobs() {
	// TODO: Print background jobs
	IMPLEMENT_ME();
	
	//Loop through the JOB QUEUE and print out the info as in the description
	//this should be pretty simple hopefully

	// Flush the buffer before returning
	fflush(stdout);
}

/***************************************************************************
 * Functions for command resolution and process setup
 ***************************************************************************/

/**
 * @brief A dispatch function to resolve the correct @a Command variant
 * function for child processes.
 *
 * This version of the function is tailored to commands that should be run in
 * the child process of a fork.
 *
 * @param cmd The Command to try to run
 *
 * @sa Command
 */
void child_run_command(Command cmd) {
	CommandType type = get_command_type(cmd);

	switch (type) {
		case GENERIC:
			run_generic(cmd.generic);
			break;

		case ECHO:
			run_echo(cmd.echo);
			break;

		case PWD:
			run_pwd();
			break;

		case JOBS:
			run_jobs();
			break;

		case EXPORT:
		case CD:
		case KILL:
		case EXIT:
		case EOC:
			break;

		default:
			fprintf(stderr, "Unknown command type: %d\n", type);
	}
}

/**
 * @brief A dispatch function to resolve the correct @a Command variant
 * function for the quash process.
 *
 * This version of the function is tailored to commands that should be run in
 * the parent process (quash).
 *
 * @param cmd The Command to try to run
 *
 * @sa Command
 */
void parent_run_command(Command cmd) {
	CommandType type = get_command_type(cmd);

	switch (type) {
		case EXPORT:
			run_export(cmd.export);
			break;

		case CD:
			run_cd(cmd.cd);
			break;

		case KILL:
			run_kill(cmd.kill);
			break;

		case GENERIC:
		case ECHO:
		case PWD:
		case JOBS:
		case EXIT:
		case EOC:
			break;

		default:
			fprintf(stderr, "Unknown command type: %d\n", type);
	}
}

/**
 * @brief Creates one new process centered around the @a Command in the @a
 * CommandHolder setting up redirects and pipes where needed
 *
 * @note Processes are not the same as jobs. A single job can have multiple
 * processes running under it. This function creates a process that is part of a
 * larger job.
 *
 * @note Not all commands should be run in the child process. A few need to
 * change the quash process in some way
 *
 * @param holder The CommandHolder to try to run
 *
 * @sa Command CommandHolder
 */
void create_process(CommandHolder holder) {
	//this method needs to push PID to the pid queue to add to a job
	
	// Read the flags field from the parser
	bool p_in  = holder.flags & PIPE_IN;
	bool p_out = holder.flags & PIPE_OUT;
	bool r_in  = holder.flags & REDIRECT_IN;
	bool r_out = holder.flags & REDIRECT_OUT;
	bool r_app = holder.flags & REDIRECT_APPEND; // This can only be true if r_out

	
	//Add comments if possible in this area
	
	if(p_out){
		pipe(enviroment_pipes[next_pipe]);
	}

	pid_t pid;
	int fd;
	pid = fork();

	if (pid == 0)
	{

		if(p_out){
			dup2(enviroment_pipes[next_pipe][WRITE_END], STDOUT_FILENO);
			close(enviroment_pipes[next_pipe][READ_END]);
		}

		if(p_in){
			dup2(enviroment_pipes[prev_pipe][READ_END], STDIN_FILENO);
			close(enviroment_pipes[prev_pipe][WRITE_END]);
		}

		if(r_in){
			char* input = holder.redirect_in;
			fd = open(input, O_RDONLY);
			dup2(fd, STDIN_FILENO);
			close(fd);
		}

		if(r_out){
			FILE* file;
			char* output = holder.redirect_out;
			if(r_app){
				file = fopen(output, "a");
				dup2(fileno(file),STDOUT_FILENO);
			}
			else	{
				file =fopen(output,"w");
				dup2(fileno(file),STDOUT_FILENO);
			}
			fclose(file);
		}

		child_run_command(holder.cmd);
		exit (EXIT_SUCCESS);
	}
	else {
		int status;
		waitpid(-1,&status,0);
		if(p_in){
			close(enviroment_pipes[prev_pipe][0]);
		}
		if(p_out){
			close(enviroment_pipes[next_pipe][WRITE_END]);
		}
		next_pipe = (next_pipe+1)%2;
		prev_pipe = (prev_pipe+1)%2;
		parent_run_command(holder.cmd);
		return;
	}
}

// Run a list of commands
void run_script(CommandHolder* holders) {
	if (holders == NULL)
		return;

	check_jobs_bg_status();

	if (get_command_holder_type(holders[0]) == EXIT &&
			get_command_holder_type(holders[1]) == EOC) {
		end_main_loop();
		return;
	}

	CommandType type;

	// Run all commands in the `holder` array
	for (int i = 0; (type = get_command_holder_type(holders[i])) != EOC; ++i)
		create_process(holders[i]);

	if (!(holders[0].flags & BACKGROUND)) {
		// Not a background Job
		// Wait for all processes under the job to complete
		int stall;
		waitpid(-1,&stall,0);
	}
	else {
		// A background job.
		// TODO: Push the new job to the job queue
		IMPLEMENT_ME();

		//this is where you add to the job queue, youll take the created PID queue
		//from above and create a job, then add the job to the JOB queue
		
		// TODO: Once jobs are implemented, uncomment and fill the following line
		// print_job_bg_start(job_id, pid, cmd);
	}
}
