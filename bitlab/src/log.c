#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "utils.h"

static struct loggers logs = { PTHREAD_MUTEX_INITIALIZER, {NULL}, 0 };

static const char* logs_dir = NULL;

const char* create_logs_dir()
{
    const char* home = getenv("HOME");
    if (home == NULL)
        return NULL;
    const char* suffix = "/.bitlab/logs";
    char* logs_dir = malloc(strlen(home) + strlen(suffix) + 1);
    if (logs_dir == NULL)
        return NULL;
    strcpy(logs_dir, home);
    strcat(logs_dir, suffix);
    return logs_dir;
}

void init_logging(const char* filename)
{
    logs.is_initializing = 1;
    if (logs_dir != NULL)
    {
        free((void*)logs_dir);
    }
    logs_dir = create_logs_dir();
    struct stat st = { 0 };

    if (stat(logs_dir, &st) == -1)
    {
        if (mkdir(logs_dir, 0700) != 0)
        {
            perror("Failed to create logs directory");
            logs.is_initializing = 0;
            free((void*)logs_dir); // Free logs_dir on error
            logs_dir = NULL;
            return;
        }
    }

    char full_path[BUFFER_SIZE];
    snprintf(full_path, sizeof(full_path), "%s/%s", logs_dir, filename);

    pthread_mutex_lock(&logs.log_mutex);
    for (int i = 0; i < MAX_LOG_FILES; ++i)
    {
        if (logs.array[i] == NULL)
        {
            logs.array[i] = (struct logger*)malloc(sizeof(logger));
            logs.array[i]->filename = (char*)malloc(strlen(full_path) + 1);
            if (logs.array[i]->filename == NULL)
            {
                perror("Failed to allocate memory for filename");
                free(logs.array[i]);
                logs.array[i] = NULL;
                break;
            }
            snprintf(logs.array[i]->filename, strlen(full_path) + 1, "%s", full_path);
            logs.array[i]->file = fopen(full_path, "a");
            if (logs.array[i]->file == NULL)
            {
                perror("Failed to open log file");
                free(logs.array[i]->filename);
                free(logs.array[i]);
                logs.array[i] = NULL;
            }
            break;
        }
    }
    logs.is_initializing = 0;
    pthread_mutex_unlock(&logs.log_mutex);
}

void log_message(log_level level, const char* filename, const char* source_file, const char* format, ...)
{
    if (logs_dir == NULL)
    {
        logs_dir = create_logs_dir();
        if (logs_dir == NULL)
        {
            fprintf(stderr, "Failed to create logs directory\n");
            return;
        }
    }

    char full_path[BUFFER_SIZE];
    snprintf(full_path, sizeof(full_path), "%s/%s", logs_dir, filename);

    pthread_mutex_lock(&logs.log_mutex);

    FILE* log = NULL;
    for (int i = 0; i < MAX_LOG_FILES; ++i)
    {
        if (logs.array[i] != NULL && !strcmp(logs.array[i]->filename, full_path))
        {
            log = logs.array[i]->file;
            break;
        }
    }

    if (log == NULL)
    {
        // Try to create and open the log file if it doesn't exist
        log = fopen(full_path, "a");
        if (log == NULL)
        {
            pthread_mutex_unlock(&logs.log_mutex);
            fprintf(stderr, "Failed to create or open log file: %s\n", full_path);
            return;
        }

        // Add the new log file to the logs array
        for (int i = 0; i < MAX_LOG_FILES; ++i)
        {
            if (logs.array[i] == NULL)
            {
                logs.array[i] = (struct logger*)malloc(sizeof(logger));
                logs.array[i]->filename = (char*)malloc(strlen(full_path) + 1);
                if (logs.array[i]->filename == NULL)
                {
                    perror("Failed to allocate memory for filename");
                    free(logs.array[i]);
                    logs.array[i] = NULL;
                    fclose(log);
                    pthread_mutex_unlock(&logs.log_mutex);
                    return;
                }
                snprintf(logs.array[i]->filename, strlen(full_path) + 1, "%s", full_path);
                logs.array[i]->file = log;
                break;
            }
        }
    }

    int timeout = 0;
    int error = 0;

    error = flock(fileno(log), LOCK_EX | LOCK_NB);
    while (error == -1 && (errno == EWOULDBLOCK || errno == EAGAIN))
    {
        usleep(LOCKED_FILE_RETRY_TIME);
        timeout += LOCKED_FILE_RETRY_TIME;

        if (timeout > LOCKED_FILE_TIMEOUT)
        {
            fprintf(stderr, "Log file locking timed out: %s\n", full_path);
            pthread_mutex_unlock(&logs.log_mutex);
            return;
        }
        error = flock(fileno(log), LOCK_EX | LOCK_NB);
    }

    fseek(log, 0, SEEK_END);

    const char* level_str;
    switch (level)
    {
    case LOG_DEBUG: level_str = "DEBUG"; break;
    case LOG_INFO: level_str = "INFO"; break;
    case LOG_WARN: level_str = "WARN"; break;
    case LOG_ERROR: level_str = "ERROR"; break;
    case LOG_FATAL: level_str = "FATAL"; break;
    default: level_str = "UNKNOWN"; break;
    }

    char timestamp[TIMESTAMP_LENGTH];
    get_formatted_timestamp(timestamp, TIMESTAMP_LENGTH);

    char message[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    fprintf(log, "%s - %s - %s - %s\n", timestamp, level_str, source_file, message);

    fflush(log);

    flock(fileno(log), LOCK_UN);

    pthread_mutex_unlock(&logs.log_mutex);
}

void finish_logging()
{
    while (logs.is_initializing)
    {
        usleep(10000); // 10 ms
    }
    pthread_mutex_lock(&logs.log_mutex);
    for (int i = 0; i < MAX_LOG_FILES; ++i)
    {
        if (logs.array[i] != NULL)
        {
            fclose(logs.array[i]->file);
            if (logs.array[i]->filename != NULL)
                free(logs.array[i]->filename);
            free(logs.array[i]);
            logs.array[i] = NULL;
        }
    }
    pthread_mutex_unlock(&logs.log_mutex);
    if (logs_dir != NULL)
    {
        free((void*)logs_dir);
        logs_dir = NULL;
    }
}
