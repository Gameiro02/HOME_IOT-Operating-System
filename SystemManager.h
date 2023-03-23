#ifndef SYSTEMMANAGER_H
#define SYSTEMMANAGER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <semaphore.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <semaphore.h>
#define BUFFER_SIZE 256

#define CONSOLE_PIPE "console_pipe"
#define SENSOR_PIPE "sensor_pipe"

typedef struct
{
    int queue_sz;
    int n_workers;
    int max_keys;
    int max_sensors;
    int max_alerts;
} Config;

struct InternalQueueNode
{
    char *sensor;
    char *key;
    char *value;

    char *command;

    int priority;
    struct InternalQueueNode *next;
};

typedef struct
{
    Config config_file;
} SharedMemory;

Config read_config_file(char *filename);

void print_config(Config config);

void inicilize_shared_memory(Config config);

void print_shared_memory();

void create_named_pipes();

void worker();

void *consol_reader_routine(void *arg);

void *sensor_reader_routine(void *arg);

void alerts_watcher();

// Variaveis globais
extern int shmid;
extern SharedMemory *shm;
extern sem_t *mutex_shm;
extern sem_t *mutex_sensor_pipe;

#endif