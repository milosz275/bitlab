#ifndef __THREAD_H
#define __THREAD_H

#include <stdio.h>
#include <pthread.h>

pthread_t thread_runner(void* (*start_routine)(void*), void* arg);

#endif // __THREAD_H
