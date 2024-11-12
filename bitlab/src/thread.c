#include "thread.h"

#include <pthread.h>
#include <errno.h>
#include <string.h>

#include "utils.h"

pthread_t thread_runner(void* (*start_routine)(void*), void* arg)
{
    pthread_t thread;
    if (pthread_create(&thread, NULL, start_routine, arg) != 0)
        log_message(LOG_WARN, BITLAB_LOG, __FILE__, "%s thread creation failed: %s", __func__, strerror(errno));
    log_message(LOG_INFO, BITLAB_LOG, __FILE__, "%s thread started", __func__);
    return thread;
}