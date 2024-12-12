#ifndef __STATE_H
#define __STATE_H

#define BITLAB_VERSION "0.1.0"

#define PEER_DISCOVERY_DEFAULT_DAEMON false
#define PEER_DISCOVERY_DEFAULT_HARDCODED_SEEDS false
#define PEER_DISCOVERY_DEFAULT_DNS_LOOKUP true

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
 * @param peer_discovery_in_progress The peer discovery operation in progress.
 * @param peer_discovery_succeeded The peer discovery operation succeeded.
 * @param peer_discovery_daemon The peer discovery operation as a daemon.
 * @param peer_discovery_hardcoded_seeds The peer discovery operation with hardcoded seeds.
 * @param peer_discovery_dns_lookup The peer discovery operation with DNS lookup.
 * @param peer_discovery_dns_domain The peer discovery DNS domain field for custom DNS lookup.
 * @param operation_mutex The mutex to protect the operation.
 */
typedef struct
{
    bool peer_discovery;
    bool peer_discovery_in_progress;
    bool peer_discovery_succeeded;
    bool peer_discovery_daemon;
    bool peer_discovery_hardcoded_seeds;
    bool peer_discovery_dns_lookup;
    char* peer_discovery_dns_domain;
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
 */
void init_program_state();

/**
 * Print the program state.
 */
void print_program_state();

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
 */
void destroy_program_state();

/**
 * Initialize the program operation.
 *
 * @return 0 if successful, otherwise 1.
 */
int init_program_operation();

/**
 * Start the peer discovery progress. This function only starts the progress if the peer discovery operation is set.
 */
void start_peer_discovery_progress();

/**
 * Finish the peer discovery progress.
 *
 * @param succeeded The flag to indicate if the peer discovery operation succeeded.
 */
void finish_peer_discovery_progress(bool succeeded);

/**
 * Set the peer discovery operation. This function cannot stop the peer discovery operation if it is in progress. Use force_stop_peer_discovery instead.
 *
 * @param value The value to set.
 * @return True if successful, otherwise false.
 */
bool set_peer_discovery(bool value);

/**
 * Force stop the peer discovery operation.
 *
 * @return True if successful, otherwise false.
 */
bool force_stop_peer_discovery();

/**
 * Get the peer discovery operation.
 *
 * @return The peer discovery operation state.
 */
bool get_peer_discovery();

/**
 * Get the peer discovery in progress state.
 *
 * @return The peer discovery in progress state.
 */
bool get_peer_discovery_in_progress();

/**
 * Get the peer discovery succeeded state.
 *
 * @return The peer discovery succeeded state.
 */
bool get_peer_discovery_succeeded();

/**
 * Set the peer discovery daemon state.
 *
 * @param value The value to set.
 * @return True if successful, otherwise false.
 */
bool set_peer_discovery_daemon(bool value);

/**
 * Set the peer discovery hardcoded seeds state.
 *
 * @param value The value to set.
 * @return True if successful, otherwise false.
 */
bool set_peer_discovery_hardcoded_seeds(bool value);

/**
 * Set the peer discovery DNS lookup state.
 *
 * @param value The value to set.
 * @return True if successful, otherwise false.
 */
bool set_peer_discovery_dns_lookup(bool value);

/**
 * Get the peer discovery daemon state.
 *
 * @return The peer discovery daemon state.
 */
bool get_peer_discovery_daemon();

/**
 * Get the peer discovery hardcoded seeds state.
 *
 * @return The peer discovery hardcoded seeds state.
 */
bool get_peer_discovery_hardcoded_seeds();

/**
 * Get the peer discovery DNS lookup state.
 *
 * @return The peer discovery DNS lookup state.
 */
bool get_peer_discovery_dns_lookup();

/**
 * Set the peer discovery DNS domain.
 *
 * @param domain The domain to set.
 * @return True if successful, otherwise false.
 */
bool set_peer_discovery_dns_domain(const char* domain);

/**
 * Get the peer discovery DNS domain.
 *
 * @return The peer discovery DNS domain.
 */
const char* get_peer_discovery_dns_domain();

/**
 * Clear all program operations.
 */
void destroy_program_operation();

/**
 * Get the program PID.
 *
 * @return The program PID.
 */
int get_pid();

/**
 * Get the program start time.
 *
 * @return The program start time.
 */
time_t get_start_time();

/**
 * Get the elapsed time since the program started.
 *
 * @return The elapsed time.
 */
int get_elapsed_time();

#endif
