#include "state.h"

#include <stdbool.h>
#include <signal.h>
#include <pthread.h>
#include <string.h>

#include "utils.h"

/**
 * The program state structure used to store the state of the program. Only accessible in state.c directly, otherwise functions should be used.
 *
 * @param pid The process ID of the program.
 * @param start_time The start time of the program.
 * @param started_with_parameters The flag to indicate if the program started with CLI parameters.
 * @param exit_flag The exit flag of the program.
 * @param exit_flag_mutex The mutex to protect the exit flag.
 */
program_state state = { 0, 0, 0, 0, PTHREAD_MUTEX_INITIALIZER };

/**
 * The program operation structure used to store the operation of the program. Only accessible in state.c directly, otherwise functions should be used.
 *
 * @param peer_discovery The peer discovery operation of the program.
 * @param peer_discovery_in_progress The peer discovery operation in progress.
 * @param peer_discovery_succeeded The peer discovery operation succeeded.
 * @param mutex The mutex to protect the operation.
 */
program_operation operation = { false, false, false, PTHREAD_MUTEX_INITIALIZER };

void init_program_state(program_state* state)
{
    state->pid = getpid();
    state->start_time = time(NULL);
    state->exit_flag = 0;
    pthread_mutex_init(&state->exit_flag_mutex, NULL);

    if (strcmp(getenv("USER"), "root") == 0)
        log_message(LOG_WARN, BITLAB_LOG, __FILE__, "Running as root is not recommended");
}

void set_exit_flag(volatile sig_atomic_t flag)
{
    pthread_mutex_lock(&state.exit_flag_mutex);
    state.exit_flag = flag;
    pthread_mutex_unlock(&state.exit_flag_mutex);
}

sig_atomic_t get_exit_flag()
{
    pthread_mutex_lock(&state.exit_flag_mutex);
    sig_atomic_t flag = state.exit_flag;
    pthread_mutex_unlock(&state.exit_flag_mutex);
    return flag;
}

void mark_started_with_parameters()
{
    pthread_mutex_lock(&state.exit_flag_mutex);
    state.started_with_parameters = true;
    pthread_mutex_unlock(&state.exit_flag_mutex);
}

void destroy_program_state(program_state* state)
{
    pthread_mutex_destroy(&state->exit_flag_mutex);
}

int init_program_operation(program_operation* operation)
{
    pthread_mutex_init(&operation->operation_mutex, NULL);
    operation->peer_discovery = false;
    return 0;
}

void start_peer_discovery_progress()
{
    pthread_mutex_lock(&operation.operation_mutex);
    if (operation.peer_discovery)
        operation.peer_discovery_in_progress = true;
    else
        log_message(LOG_WARN, BITLAB_LOG, __FILE__, "Peer discovery operation not set, cannot start progress");
    pthread_mutex_unlock(&operation.operation_mutex);
}

void finish_peer_discovery_progress(bool succeeded)
{
    pthread_mutex_lock(&operation.operation_mutex);
    operation.peer_discovery = false;
    operation.peer_discovery_in_progress = false;
    operation.peer_discovery_succeeded = succeeded;
    pthread_mutex_unlock(&operation.operation_mutex);
}

int set_peer_discovery(bool value)
{
    pthread_mutex_lock(&operation.operation_mutex);
    if (operation.peer_discovery && operation.peer_discovery_in_progress && !value)
        log_message(LOG_WARN, BITLAB_LOG, __FILE__, "Peer discovery operation in progress, cannot stop. Did you mean force_stop_peer_discovery?");
    else
        operation.peer_discovery = value;
    pthread_mutex_unlock(&operation.operation_mutex);
    return 0;
}

int force_stop_peer_discovery()
{
    pthread_mutex_lock(&operation.operation_mutex);
    operation.peer_discovery = false;
    operation.peer_discovery_in_progress = false;
    log_message(LOG_WARN, BITLAB_LOG, __FILE__, "Peer discovery operation force-stopped");
    pthread_mutex_unlock(&operation.operation_mutex);
    return 0;
}

bool get_peer_discovery()
{
    pthread_mutex_lock(&operation.operation_mutex);
    bool value = operation.peer_discovery;
    pthread_mutex_unlock(&operation.operation_mutex);
    return value;
}

bool get_peer_discovery_in_progress()
{
    pthread_mutex_lock(&operation.operation_mutex);
    bool value = operation.peer_discovery_in_progress;
    pthread_mutex_unlock(&operation.operation_mutex);
    return value;
}

void destroy_program_operation(program_operation* operation)
{
    pthread_mutex_destroy(&operation->operation_mutex);
}

int get_pid()
{
    return state.pid;
}

time_t get_start_time()
{
    return state.start_time;
}

int get_elapsed_time()
{
    return time(NULL) - state.start_time;
}
