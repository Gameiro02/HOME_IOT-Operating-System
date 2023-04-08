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
#include <sys/msg.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#define WORKER_TO_CONSOLE 2
#define ALERTS_TO_CONSOLE 3

#define BUFFER_SIZE 1024

#define CONSOLE_PIPE "console_pipe"
#define SENSOR_PIPE "sensor_pipe"

#define WRITE 1
#define READ 0

#define QUEUE_SIZE 1024

#define QUEUE_KEY 1234

typedef struct
{
    int queue_sz;
    int n_workers;
    int max_keys;
    int max_sensors;
    int max_alerts;
} Config;

struct alert_list_node
{
    char id[BUFFER_SIZE];
    char key[BUFFER_SIZE];
    int min_value;
    int max_value;
};

struct queue
{
    struct alert_list_node data[QUEUE_SIZE];
    int front;
    int rear;
    int size;
};

struct key_list_node
{
    char key[BUFFER_SIZE];
    char sensor_id[BUFFER_SIZE];
    int last_value;
    int min_value;
    int max_value;
    double avg_value;
    int num_updates;
};

struct key_queue
{
    struct key_list_node data[QUEUE_SIZE];
    int front;
    int rear;
    int size;
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
    int num_alerts_added;

    struct key_queue key_list;
    struct queue alert_queue;

} SharedMemory;

typedef struct
{
    long type;
    char message[4096];
} message;

Config read_config_file(char *filename);
bool is_user_command(char *msg);
void print_config(Config config);
void handle_sigint();
void inicilize_shared_memory(Config config);

void print_shared_memory();

void create_named_pipes();

void worker(int worker_id, int read_pipe);
void *consol_reader_routine();

void *sensor_reader_routine();

void alerts_watcher();

bool push_sensor_message_to_internal_queue(struct InternalQueueNode **head, char *sensor, char *key, int value, char *command, int priority);

void print_internal_queue(struct InternalQueueNode *head);

struct InternalQueueNode *pop(struct InternalQueueNode **head);

struct InternalQueueNode parse_params(const char *str);

char *create_msg_to_worker(struct InternalQueueNode *node);

bool process_command_worker(const char *buffer, int worker_id);

bool check_msg(char *str);

bool add_alert(struct alert_list_node **head, char *id, char *key, int min_value, int max_value);

bool remove_alert(struct alert_list_node **head, char *id);

void list_alerts(struct alert_list_node *head);

void init_queue(struct queue *q);
int is_empty(struct queue *q);
int is_full(struct queue *q);
bool enqueue(struct queue *q, struct alert_list_node data);
bool dequeue_by_id(struct queue *q, char *id);
struct alert_list_node create_alert_list_node(char *id, char *key, int min_value, int max_value);
void print_queue(struct queue *q);

void init_key_queue(struct key_queue *q);
int is_key_empty(struct key_queue *q);
int is_key_full(struct key_queue *q);
void enqueue_key(struct key_queue *q, char *key, int value);
struct key_list_node dequeue_key(struct key_queue *q);
bool reset_keys(struct key_queue *q);
void print_key_list(struct key_queue *q);
void print_key_names(struct key_queue *q);

// Variaveis globais
extern int shmid;
extern SharedMemory *shm;
extern sem_t *mutex_shm;
extern sem_t *log_sem;
extern sem_t *key_list_empty_sem;
extern sem_t *worker_status_sem;
extern int msg_queue_id;
extern struct InternalQueueNode *internal_queue;
extern pthread_mutex_t internal_queue_mutex;

#endif