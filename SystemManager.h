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

#define WRITE 1
#define READ 0

typedef struct
{
    int queue_sz;
    int n_workers;
    int max_keys;
    int max_sensors;
    int max_alerts;
} Config;

struct key_list_node
{
    char key[BUFFER_SIZE];
    int last_value;
    int min_value;
    int max_value;
    double avg_value;
    int num_updates;

    struct key_list_node *next;
};

struct InternalQueueNode
{
    char sensor_id[BUFFER_SIZE];
    char key[BUFFER_SIZE];
    int value;

    char command[BUFFER_SIZE];

    int priority;
    struct InternalQueueNode *next;
};

typedef struct
{
    Config config_file;
    int *workers_status; // 0 - not working, 1 - working

    int num_keys_added;
    struct key_list_node *key_list;

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

bool push_sensor_message_to_internal_queue(struct InternalQueueNode **head, char *sensor, char *key, int value, char *command, int priority);

void print_internal_queue(struct InternalQueueNode *head);

struct InternalQueueNode *pop(struct InternalQueueNode **head);

struct InternalQueueNode parse_params(const char *str);

char *create_msg_to_worker(struct InternalQueueNode *node);

void push_key_list(struct key_list_node **head, char *key, int value);

bool update_key_list(struct key_list_node **head, char *key, int value);

void print_key_list(struct key_list_node *head);

bool process_command_worker(const char *buffer, int worker_id);

void print_key_list_to_user();

// Variaveis globais
extern int shmid;
extern SharedMemory *shm;
extern sem_t *mutex_shm;
extern sem_t *mutex_sensor_pipe;

#endif