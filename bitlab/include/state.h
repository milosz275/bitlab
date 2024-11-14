#ifndef __STATE_H
#define __STATE_H

#include <signal.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <pthread.h>

/**
 * The program state structure used to store the state of the program.
 *
 * @param pid The process ID of the program.
 * @param start_time The start time of the program.
 * @param started_with_parameters The flag to indicate if the program started with CLI parameters.
 * @param exit_flag The exit flag of the program.
 * @param exit_flag_mutex The mutex to protect the exit flag.
 */
typedef struct
{
    pid_t pid;
    time_t start_time;
    bool started_with_parameters;
    volatile sig_atomic_t exit_flag;
    pthread_mutex_t exit_flag_mutex;
} program_state;

/**
 * The program operation structure used to store the operation of the program.
 *
 * @param peer_discovery The peer discovery operation of the program.
 * @param operation_mutex The mutex to protect the operation.
 * @param peer_discovery_in_progress The peer discovery operation in progress.
 */
typedef struct
{
    bool peer_discovery;
    bool peer_discovery_in_progress;
    pthread_mutex_t operation_mutex;
} program_operation;

/**
 * The program state used to store the state of the program.
 */
extern program_state state;

/**
 * The program operation used to store all current operations of the program.
 */
extern program_operation operation;

/**
 * Initialize the program state.
 *
 * @param state The program state to initialize.
 */
void init_program_state(program_state* state);

/**
 * Set the exit flag.
 *
 * @param flag The flag to set.
 */
void set_exit_flag(volatile sig_atomic_t flag);

/**
 * Get the exit flag.
 *
 * @return The exit flag.
 */
sig_atomic_t get_exit_flag();

/**
 * Mark the program as started with parameters.
 */
void mark_started_with_parameters();

/**
 * Destroy the program state.
 *
 * @param state The program state to destroy.
 */
void destroy_program_state(program_state* state);

/**
 * Initialize the program operation.
 *
 * @param operation The program operation to initialize.
 * @return 0 if successful, otherwise 1.
 */
int init_program_operation(program_operation* operation);

/**
 * Set the peer discovery operation.
 *
 * @param value The value to set.
 * @return 0 if successful, otherwise 1.
 */
int set_peer_discovery(bool value);

/**
 * Get the peer discovery operation.
 *
 * @return The peer discovery operation state.
 */
bool get_peer_discovery();

/**
 * Clear all program operations.
 *
 * @param operation The program operation to clear.
 */
void destroy_program_operation(program_operation* operation);

#endif
