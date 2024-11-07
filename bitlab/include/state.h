#ifndef __STATE_H
#define __STATE_H

#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>

/**
 * The program state structure used to store the state of the program.
 *
 * @param pid The process ID of the program.
 * @param start_time The start time of the program.
 * @param exit_flag The exit flag of the program.
 */
typedef struct
{
    pid_t pid;
    time_t start_time;
    volatile sig_atomic_t exit_flag;
} program_state;

extern volatile program_state state;

void init_program_state(volatile program_state* state);

void set_exit_flag(volatile program_state* state, sig_atomic_t flag);

sig_atomic_t get_exit_flag(volatile program_state* state);

#endif
