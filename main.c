// Author:  Mitch Campbell | campbmit@oregonstate.edu
// Course:  CS344
// Project: Project 3 - smallsh

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

// #include "userCommand.h"


// CONSTANTS
#define MAX_STR_SIZE    2048
#define MAX_ARGS        512


// FLAGS
#define DEBUG false


// GLOBALS
// Foreground-only mode switch
int fg_only = 0;


// FUNCTION DECLARATIONS
// void run_child(struct userCommand *uc);

// Struct: argList
//
// Store list of arguments passed by user 
// as a node in a linked list.
struct argList
{
    char *argument;
    struct argList *next;
};

// Function: add_arg()
// 
// Adds a node to end of linked list.
//      
//      old_tail:   pointer to last node in an argList
//      arg_token:  pointer to string containing argument to be added to argList
struct argList *add_arg(struct argList *old_tail, char *arg_token)
{
    
    // Create new tail node containing argument string
    struct argList *new_tail = calloc(1, sizeof( struct argList ));
    new_tail->argument = calloc(1, sizeof(strlen(arg_token) + 1));
    strcpy(new_tail->argument, arg_token);
    new_tail->next = NULL;

    // Point old tail to new tail node
    old_tail->next = new_tail;

    return(new_tail);
};

// Struct: user_command
// 
// Stores a parsed command-line string supplied to smallsh
struct userCommand
{
    // Stores command string
    char *command;

    // Stores pointer to linked list of arguments
    struct argList *arguments;

    // Store piping information
    bool pipe_in;
    char *infile_name;
    bool pipe_out;
    char *outfile_name;

    // Background process toggle
    bool background;
};

// Function: arg_pointer_array()
// 
// Parses a userCommand into an array of argument pointers 
// to be passed to exec() functions
//      
//      uc:   pointer to a userCommand
char ** arg_pointer_array(struct userCommand *uc)
{

    char *arg_ptrs[1 + MAX_ARGS + 1];

    // Set command path as first argument
    arg_ptrs[0] = uc->command;

    // Traverse userCommand arguments, storing pointers sequentially
    struct argList *curr_arg = uc->arguments;
    int counter = 1;
    while(curr_arg != NULL)
    {
        arg_ptrs[counter] = curr_arg->argument;
        counter++;
        curr_arg = curr_arg->next;
    }

    // Set NULL pointer in final position
    arg_ptrs[counter] = NULL;

    return(arg_ptrs);
}

// Function: expand_vars()
// 
// Expands variables ($$) in user command string to smallsh's process id
//      
//      cl_string:  pointer to user's command-line string
char *expand_vars(char *cl_string)
{

    int var_count = 0;
    for(int i = 0; i < strlen(cl_string) - 1; i++)
    {
        if(cl_string[i] == '$' && cl_string[i+1] == '$')
        {
            var_count++;
            i++;
        }
    }

    // No variables present
    if(var_count == 0)
    {
        return(cl_string);
    }

    // Expand variables in cl_string into new string buffer
    else{

        // Initialize empty string buffer
        char *new_cl_string = calloc(strlen(cl_string) + (16 * var_count), sizeof(char));
        new_cl_string[0] = '\0';

        // Store process id in string
        char pid[11];
        memset(pid, 0, 11);
        sprintf(pid, "%d", getpid());

        // Write chars sequentially from cl_string to new_cl_string
        for(int i = 0; i < strlen(cl_string) - 1; i++)
        {
            // Variable encountered. 
            // Write process id to new_cl_string and skip next char
            if( cl_string[i] == '$' && cl_string[i+1] == '$')
            {
                strcat(new_cl_string, pid);
                i++;
            }

            // Variable not encountered. 
            // Write char new_cl_string
            else
            {
                int ncls_length = strlen(new_cl_string);
                new_cl_string[ncls_length] = cl_string[i];
                new_cl_string[ncls_length + 1] = '\0';
            }
        }

        return (new_cl_string);
    }

}

// Function: get_command()
// 
// Gets command-line input as input.
//
//      cl_string: pointer to string buffer
int get_command(char *cl_string)
{
    printf(": ");
    fflush(stdout);
    
    fgets(cl_string, 2050, stdin);

    return(0);
}

// Function: parse_command()
// 
// Parses a command-line string into a userCommand struct
//      
//      cl_string:  pointer to user's command-line string
struct userCommand *parse_command(char *cl_string) 
{

    struct userCommand *parsed_command = calloc(1, sizeof(struct userCommand));
    
    // Zero-out piping and background flags
        parsed_command->pipe_in = false;
        parsed_command->pipe_out = false;
        parsed_command->background = false;
    
    // For use with strtok_r
    char *saveptr;
    char *token;

    // Store first word as command
    token = strtok_r(cl_string, " ", &saveptr);
    parsed_command->command = calloc(strlen(token) + 1, sizeof(char));
    strcpy(parsed_command->command, token);
    
    // Set next word as first argument
    token = strtok_r(NULL, " ", &saveptr);
    struct argList *arg_list_tail = calloc(1, sizeof(struct argList));
    
    // Check for special non-argument words
    if(
        token != NULL 
        && strcmp(token, ">") != 0 
        && strcmp(token, "<") != 0)
    {
        arg_list_tail->argument = calloc(strlen(token) + 1, sizeof(char));
        strcpy(arg_list_tail->argument, token);
        parsed_command->arguments = arg_list_tail;

        token = strtok_r(NULL, " ", &saveptr);
    }
    
    // Handle remaining arguments

    // While input and output redirection not encountered
    while( 
        token != NULL 
        && strcmp(token, ">") != 0 
        && strcmp(token, "<") != 0
    )
    {
        // If "&" found, check first whether it is at end of command-line (next token is NULL).
        // If not at end, add "&" as a normal word, else set background flag if not in fg-only mode.
        if(strcmp(token, "&") == 0)
        {
            token = strtok_r(NULL, " ", &saveptr);
            if(token != NULL)
            {
                arg_list_tail = add_arg(arg_list_tail, "&");
            }
            else if(!fg_only)
            {
                parsed_command->background = true;
            }
        }
        // "&" not found, process as normal word
        else
        {
            arg_list_tail = add_arg(arg_list_tail, token);
            token = strtok_r(NULL, " ", &saveptr);
        }
    }

    // Set input/output redirection flags if found
    while(token != NULL)
    {
        
        // Set input pipe flag and source
        if (strcmp(token, "<" ) == 0 ){
            token = strtok_r(NULL, " ", &saveptr);
            parsed_command->infile_name = calloc(strlen(token) + 1, sizeof(char));
            strcpy(parsed_command->infile_name, token);
            parsed_command->pipe_in = true;
        }

        // Set output pipe flag and destination
        else if (strcmp(token, ">" ) == 0 ){
            token = strtok_r(NULL, " ", &saveptr);
            parsed_command->outfile_name = calloc(strlen(token) + 1, sizeof(char));
            strcpy(parsed_command->outfile_name, token);
            parsed_command->pipe_out = true;
        }

        // Set background process flag
        else if (strcmp(token, "&") == 0 && !fg_only)
        {
            parsed_command->background = true;
        }

        // Unset background process flag
        else if (strcmp(token, "&") == 0 && fg_only)
        {
            parsed_command->background = true;
        }

        // Advance to next word
        token = strtok_r(NULL, " ", &saveptr);

    }

    return parsed_command;
}

// Function: execute_user_command()
// 
// Executes a command received by smallsh.
//      
//      uc:                 parsed command
//      fg_child_status:    pointer to foreground child process status storage
int execute_user_command(struct userCommand *uc, int *fg_child_status)
{

    // Handle built-in commands (exit, status, cd)

    // Handle exit command
    // Returns to main, where final cleanup of child processes will occur
    if(strcmp(uc->command, "exit") == 0)
    {
        return(0);
    }

    // Handle status command
    // Checks status code left by most recently closed foreground process
    if(strcmp(uc->command, "status") == 0)
    {
        // Check if most recent foreground child process exited normally.
        // If so, interpret and print exit status
        if(WIFEXITED(*fg_child_status))
        {
            printf("Most recent fg process exited with status of: %d\n", WEXITSTATUS(*fg_child_status));
            fflush(stdout);
        }
        // If most recent foreground child process exited abnormally (due to signal),
        // interpret and print terminating signal
        else
        {
            printf("Most recent fg process terminated by signal (%d)\n", WTERMSIG(*fg_child_status));
            fflush(stdout);
        }
        return(0);
    }

    // Handle cd command
    else if(strcmp(uc->command, "cd") == 0)
    {
        // No argument passed, move to home directory
        if(uc->arguments == NULL)
        {
            chdir(getenv("HOME"));
        }
        // Move to directory passed as first argument
        else
        {
            chdir(uc->arguments->argument);
        }
        return(0);
    }

    // Handle non-built-in commands
    else
    {

        // Create child process
        pid_t child_pid = fork();

        switch(child_pid)
        {
            // Child fails to spawn
            case -1: ;
                printf("Error: child failed to spawn\n");
                fflush(stdout);

                break;

            // Child process branch
            case 0: ;

                // Set child process to ignore SIGTSTP signal
                signal(SIGTSTP, SIG_IGN);

                // Set foreground child processes not to ignore SIGINT signal
                if(uc->background == false)
                {
                    signal(SIGINT, SIG_DFL);
                }

                // Execute command and then exit
                run_child(uc);
                exit(0);

                break;

            // Parent process branch
            default: ;

                // Handle background child process
                if(uc->background == true){
                    printf("Child process started, pid = %d\n", child_pid);
                    fflush(stdout);

                    return(child_pid);
                }

                // Handle foreground child process
                if(uc->background == false){
                    
                    // Wait for foreground child process to finish, 
                    // store exit status/terminating signal
                    child_pid = waitpid(child_pid, fg_child_status, 0);

                    // If terminated by signal, report
                    if(WIFSIGNALED(*fg_child_status))
                    {
                        printf("Terminated with signal (%d)\n", WTERMSIG(*fg_child_status));
                        fflush(stdout);
                    }
                    return(0);
                }
        }
    
    }

    return(0);

}

// Function: run_child()
// 
// Executes user command in current child process using function from exec() family
//      
//      uc:     command to execute
void run_child(struct userCommand *uc){

    // Create array of arguments to be passed to execv()
    char **arg_array = arg_pointer_array(uc);

    // For use in handling input/output redirection
    int infile;
    int outfile;

    // Handle input redirection

    // Redirect background process input to /dev/null (if no input file specified)
    if(!uc->pipe_in && uc->background)
    {
        // Open /dev/null for reading
        infile = open("/dev/null", O_RDONLY);
        // Force file pointer pointing to stdout to point to /dev/null
        int result = dup2(infile, 0);
        // If redirect failed, report error
        if(result == -1)
        {
            fprintf(stderr, "Error, input redirection failed\n");
            fflush(stdout);
            exit(1);
        }
    }

    // Redirect child process input to file specified by user
    if(uc->pipe_in)
    {
        // Open specified file for reading
        infile = open(uc->infile_name, O_RDONLY);
        // Force file pointer pointing to stdout to point to specified file
        int result = dup2(infile, 0);
        // If redirect failed, report error
        if(result == -1)
        {
            printf("Error, input redirection failed\n");
            fflush(stdout);
            exit(1);
        }
    }
    
    // Handle output redirection

    // Redirect background process input to /dev/null (if no outout file specified)
    if(!uc->pipe_out && uc->background)
    {
        // Open /dev/null for reading
        outfile = open("/dev/null", O_WRONLY | O_CREAT | O_TRUNC, 0644);
        // Force file pointer pointing to stdout to point to /dev/null
        int result = dup2(outfile, 1);
        // If redirect failed, report error
        if(result == -1)
        {
            printf("Error, output redirection failed\n");
            fflush(stdout);
            exit(1);
        }
    }
    if(uc->pipe_out)
    {
        // Open specified file for reading
        outfile = open(uc->outfile_name, O_WRONLY | O_CREAT | O_TRUNC, 0666);
        // If file open fails, report error
        if(outfile == -1)
        {
            printf("Error, output file descriptor creation failed");
            fflush(stdout);
            exit(1);
        }
        // Force file pointer pointing to stdout to point to specified file
        int result = dup2(outfile, 1);
        // If redirect failed, report error
        if(result == -1)
        {
            printf("Error, output redirection failed\n");
            fflush(stdout);
            exit(1);
        }
    }
    
    // Try executing command on path
    int on_path = execvp(uc->command, arg_array);
    
    // If command not found on path, try executing in cwd
    int off_path = 1;
    if(!on_path){
        off_path = execv(uc->command, arg_array);
    }

    // If command not found on path or local, report error
    if(on_path && off_path)
    {
        printf("Error: couldn't find command\n");
        exit(1);
    }

    exit(0);
}

// Function: decon_user_command()
// 
// Frees memory allocated on heap (malloc/calloc) for a userCommand
//      
//      uc:     userCommand struct to be freed
void decon_user_command(struct userCommand *uc)
{

    free(uc->command);

    if( uc->infile_name != NULL ){
        free(uc->infile_name);
    }
    if( uc->outfile_name != NULL ){
        free(uc->outfile_name);
    }

    // Free memory allocated for arguments (linked list)
    if( uc->arguments != NULL ){
        decon_arg_list(uc->arguments);
    }

    // Finally, free memory allocated for userCommand struct
    free(uc);
}

// Function: decon_arg_list()
// 
// Recursively frees memory allocated on heap (malloc/calloc) for an argList.
// Memory for all nodes further down the linkedlist will also be freed.
//      
//      curr_arg:     node of argList to be 
void decon_arg_list(struct argList *curr_arg)
{
    // If not at end of linked list, recurse on ->next
    if(curr_arg->next != NULL)
    {
        decon_arg_list(curr_arg->next);
    }
    
    // After all nodes further down list free, free self
    free(curr_arg->argument);
}

// Function: catch_SIGTSTP()
// 
// Toggles foreground-only mode.
// This function is registered to parent process's SIGTSTP signal
//      
//      signal_num:     number of caught signal
void catch_SIGTSTP(int signal_num)
{
    // Toggle foreground-only mode
    switch(fg_only)
    {
        // Turn on
        case 0:
            fg_only = 1;
            
            // Report mode change
            int status = write(STDOUT_FILENO, "\nEntering foreground-only mode (& is now ignored)\n", 52);
            break;

        // Turn off
        case 1:
            fg_only = 0;

            // Report mode change
            write(STDOUT_FILENO, "\nExiting foreground-only mode\n", 32);
            break;
    }
}

// Function: main()
// 
// Start smallsh
int main(void)
{

    // Set initial signal actions

        struct sigaction SIGTSTP_action = {0};

        // Create signal action for toggling foreground-only mode
        SIGTSTP_action.sa_handler = catch_SIGTSTP;
        sigfillset(&SIGTSTP_action.sa_mask);
        SIGTSTP_action.sa_flags = SA_RESTART;

    // Register signal actions
    
        // Start toggling of foreground mode on SIGTSTP signal
        sigaction(SIGTSTP, &SIGTSTP_action, NULL);
        // Start ignoring SIGINT signal
        signal(SIGINT, SIG_IGN);

    // Store most recent foreground child exit status
    int fg_child_status = 0;

    // Create array to store pids of child processes
    int child_ids[512] = {0};
    //memset(child_ids, 0, 512 * sizeof(int));

    // Create buffer to store command-line string
    char *cl_string = calloc(MAX_STR_SIZE + 2, sizeof(char));

    // Get commands from user until "exit" received
    do
    {

        // Clean up any child processes that are no longer running
        for(int i = 0; i < 512; i++)
        {
            
            // Check each recorded background child process to see if it has exited.
            // If any child process has ended, report its exit status OR its terminating signal
            // and then remove it from the array
            int child_status;
            if(child_ids[i] != 0)
            {
                // Child process has ended
                if(waitpid(child_ids[i], &child_status, WNOHANG) != 0)
                {
                    // If exited normally, report exit status
                    if(WIFEXITED(child_status))
                    {
                        printf("Process %d exited with status (%d)\n", child_ids[i], WEXITSTATUS(child_status));
                        fflush(stdout);
                    }
                    
                    // If exited abnormally, report terminating signal
                    if(WIFSIGNALED(child_status))
                    {
                        printf("Process %d terminated by signal (%d)\n", child_ids[i], WTERMSIG(child_status));
                        fflush(stdout);
                    }

                    // Remove child's pid from array, if ended
                    child_ids[i] = 0;
                }
            }
        }

        // Store user input command into string buffer
        get_command(cl_string);

        // Handle user input that is blank line and comment
        if( cl_string[0] == '\n' || cl_string[0] == '#'){
            memset(cl_string, 0, MAX_STR_SIZE + 2);
            continue;
        }

        // Null-terminate command-line string
        cl_string[strlen(cl_string) - 1] = '\0';

        // Expand variables in command-line string
        if(strlen(cl_string) > 1){
            cl_string = expand_vars(cl_string);
        }

        // Parse command-line string
        struct userCommand *user_command = parse_command(cl_string);

        // Attempt to execute user command
        int new_child_id = execute_user_command(user_command, &fg_child_status);

        // If background process was created, store child's process id for checking later
        if(user_command->background)
        {
            // Find next free space in child_ids array
            int counter = 0;
            while(child_ids[counter] != 0)
            {
                counter++;
            }

            // Store child pid
            child_ids[counter] = new_child_id;
        }

        // Deconstruct (deallocate memory for) user_command
        decon_user_command(user_command);

    }
    // "exit" command received, exit smallsh
    while(strcmp(cl_string, "exit") != 0);

    // Loop through array of child pids, terminating all remaining running child processes
    for(int i = 0; i < 512; i++)
    {
        if(child_ids[i] != 0)
        {
            kill(child_ids[i], SIGKILL);
        }
    }

    exit(0);
}