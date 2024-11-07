#include "state.h"

#include <signal.h>

volatile program_state state = { 0, 0, 0 };

void init_program_state(volatile program_state* state)
{
    state->pid = getpid();
    state->start_time = time(NULL);
    state->exit_flag = 0;
}

void set_exit_flag(volatile program_state* state, sig_atomic_t flag)
{
    state->exit_flag = flag;
}

sig_atomic_t get_exit_flag(volatile program_state* state)
{
    return state->exit_flag;
}
