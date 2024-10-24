#ifndef __LOG_H
#define __LOG_H

#include <stdio.h>
#include <stdarg.h>
#include <pthread.h>

#define MAX_LOG_FILES 10
#define MAX_FILENAME_LENGTH 256
#define LOGS_DIR "logs"
#define BITLAB_LOG "bitlab.log"
#define LOG_BITLAB_STARTED "BitLab started ----------------------------------------------------------------------------------------"
#define LOG_BITLAB_FINISHED "BitLab finished successfully"

#define LOCKED_FILE_RETRY_TIME 1000 // in microseconds (1 millisecond)
#define LOCKED_FILE_TIMEOUT 5000000 // in microseconds (5 seconds)

/**
 * The log level enumeration used to define the level of logging that is being used.
 *
 * @param LOG_DEBUG Debug level logging
 * @param LOG_INFO Information level logging
 * @param LOG_WARN Warning level logging
 * @param LOG_ERROR Error level logging
 * @param LOG_FATAL Fatal level logging
 */
typedef enum
{
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_FATAL
} log_level;

/**
 * The logger structure used to store the logger for the logging system.
 *
 * @param filename The filename of the log file.
 * @param file The file pointer for the log file.
 */
typedef struct logger
{
    char* filename;
    FILE* file;
} logger;

/**
 * The loggers structure used to store the loggers for the logging system.
 *
 * @param log_mutex The mutex for the loggers.
 * @param array The array of loggers.
 * @param is_initializing The flag to indicate if the loggers are initializing.
 */
typedef struct loggers
{
    pthread_mutex_t log_mutex;
    logger* array[MAX_LOG_FILES];
    int is_initializing;
} loggers;

/**
 * Initialize logging used to initialize the logging system, open and preserve the log file.
 *
 * @param log_file The log file to write to.
 */
void init_logging(const char* filename);

/**
 * Log a message used to log a message to the console or a file.
 *
 * @param level The log level.
 * @param log_file The file that the log message is destined for.
 * @param source_file The source file that the log message is from.
 * @param format The format of the log message.
 * @param ... The arguments for the format.
 */
void log_message(log_level level, const char* filename, const char* source_file, const char* format, ...);

/**
 * Finish logging used to finish logging and close the log file.
 */
void finish_logging();

#endif // __LOG_H
