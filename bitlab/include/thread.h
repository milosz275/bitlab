#ifndef __THREAD_H
#define __THREAD_H

#include <stdio.h>
#include <pthread.h>

/**
 * Thread runner function. This function is used to create a new thread.
 *
 * @param start_routine The function pointer to the start routine.
 * @param name The name of the thread.
 * @param arg The argument to pass to the start routine.
 */
pthread_t thread_runner(void* (*start_routine)(void*), const char* name, void* arg);

#endif // __THREAD_H
