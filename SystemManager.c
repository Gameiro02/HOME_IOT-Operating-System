#include "SystemManager.h"
#include "log.h"

// Variaveis globais
int shmid;
SharedMemory *shm;
sem_t *mutex_shm;
sem_t *mutex_sensor_pipe;

void inicilize_shared_memory(Config config)
{
    sem_wait(mutex_shm);
    shm->config_file = config;
    sem_post(mutex_shm);
}

void print_shared_memory()
{
    sem_wait(mutex_shm);

    printf("Queue Size: %d\n", shm->config_file.queue_sz);
    printf("Number of Workers: %d\n", shm->config_file.n_workers);
    printf("Max Keys: %d\n", shm->config_file.max_keys);
    printf("Max Sensors: %d\n", shm->config_file.max_sensors);
    printf("Max Alerts: %d\n", shm->config_file.max_alerts);

    sem_post(mutex_shm);
}

void worker(int i)
{
    char *message = malloc(100);
    sprintf(message, "WORKER %d CREATED", i);
    write_log(message);
    free(message);
}

void *sensor_reader_routine(void *arg)
{
    write_log("THREAD SENSOR_READER CREATED");

    int fd;
    char buffer[BUFFER_SIZE];
    int read_bytes;

    // Abre a pipe SENSOR_PIPE para leitura
    if ((fd = open(SENSOR_PIPE, O_RDONLY)) == -1)
    {
        perror("Erro ao abrir a SENSOR_PIPE");
        pthread_exit(NULL);
    }

    while (1)
    {
        read_bytes = read(fd, buffer, BUFFER_SIZE);

        // Le a mensagem da pipe
        if (read_bytes == -1)
        {
            perror("Erro ao ler da SENSOR_PIPE");
            pthread_exit(NULL);
        }

        // Se a mensagem for "exit" termina a thread
        if (strcmp(buffer, "exit") == 0)
        {
            break;
        }

        // Imprime a mensagem
        if (read_bytes > 0)
            printf("Sensor: %s \n", buffer);
        bzero(buffer, BUFFER_SIZE);
    }
    // Fecha a pipe e finaliza a thread
    close(fd);
    pthread_exit(NULL);
}

void *console_reader_routine(void *arg)
{
    write_log("THREAD CONSOLE_READER CREATED");

    int fd;
    char buffer[BUFFER_SIZE];
    int read_bytes;

    // Abre a pipe SENSOR_PIPE para leitura
    if ((fd = open(CONSOLE_PIPE, O_RDONLY)) == -1)
    {
        perror("Erro ao abrir a CONSOLE_PIPE");
        pthread_exit(NULL);
    }

    while (1)
    {
        read_bytes = read(fd, buffer, BUFFER_SIZE);

        // Le a mensagem da pipe
        if (read_bytes == -1)
        {
            perror("Erro ao ler da SENSOR_PIPE");
            pthread_exit(NULL);
        }

        // Se a mensagem for "exit" termina a thread
        if (strcmp(buffer, "exit") == 0)
        {
            break;
        }

        // Imprime a mensagem
        if (read_bytes > 0)
            printf("User Console: %s \n", buffer);
        bzero(buffer, BUFFER_SIZE);
    }
    // Fecha a pipe e finaliza a thread
    close(fd);
    pthread_exit(NULL);
}

void alerts_watcher()
{
    printf("Alerts Watcher created\n");
}

void push_sensor_message_to_internal_queue(struct InternalQueueNode *queue, char *message)
{
    // Parse the message: Senseor, Key, Value
    char *sensor = strtok(message, ",");
    char *key = strtok(NULL, ",");
    char *value = strtok(NULL, ",");

    // Create the node
    struct InternalQueueNode *node = (struct InternalQueueNode *)malloc(sizeof(struct InternalQueueNode));

    strcpy(node->sensor, sensor);
    strcpy(node->key, key);
    strcpy(node->value, value);

    node->priority = 0;

    node->next = NULL;

    // Push the node to the queue
    if (queue->next == NULL)
    {
        queue->next = node;
    }
    else
    {
        struct InternalQueueNode *aux = queue->next;

        while (aux->next != NULL)
        {
            aux = aux->next;
        }

        aux->next = node;
    }
}

void push_console_message_to_internal_queue(struct InternalQueueNode *queue, char *message)
{
    // to the console message we only need to copy the message to the command field
    struct InternalQueueNode *node = (struct InternalQueueNode *)malloc(sizeof(struct InternalQueueNode));

    strcpy(node->command, message);

    node->priority = 1;

    node->key = NULL;
    node->sensor = NULL;
    node->value = NULL;

    node->next = NULL;

    // Push the node to the queue
    if (queue->next == NULL)
    {
        queue->next = node;
    }
    else
    {
        struct InternalQueueNode *aux = queue->next;

        while (aux->next != NULL)
        {
            aux = aux->next;
        }

        aux->next = node;
    }
}

void pull_internal_queue(struct InternalQueueNode *queue)
{
    struct InternalQueueNode *aux = queue->next;

    if (aux != NULL)
    {
        queue->next = aux->next;
        free(aux);
    }
}

int main()
{
    write_log("HOME_IOT SIMULATOR STARTING");

    Config config = read_config_file("config.txt");

    // Create shared memory
    int shmid = shmget(IPC_PRIVATE, sizeof(SharedMemory), IPC_CREAT | 0777);
    if (shmid < 0)
    {
        perror("shmget: ");
        exit(1);
    }

    // Attach shared memory
    shm = (SharedMemory *)shmat(shmid, NULL, 0);

    if (shm == NULL)
    {
        perror("shmat: ");
        exit(1);
    }

    // Create semaphore posix
    mutex_shm = sem_open("mutex_shm", O_CREAT, 0777, 1);
    if (mutex_shm == SEM_FAILED)
    {
        perror("sem_open: ");
        exit(1);
    }

    // Create the log sem

    // Create the INTERNAL_QUEUE sem

    inicilize_shared_memory(config);

    create_named_pipes();

    // Create workers
    for (int i = 0; i < config.n_workers; i++)
    {
        if (fork() == 0)
        {
            worker(i);
            exit(0);
        }
    }

    // Create the Alerts Watcher process
    if (fork() == 0)
    {
        alerts_watcher();
        exit(0);
    }

    // Create the 2 threads: console_reader and sensor_reader
    pthread_t console_reader, sensor_reader, dispatcher;

    pthread_create(&sensor_reader, NULL, sensor_reader_routine, NULL);
    pthread_create(&console_reader, NULL, console_reader_routine, NULL);

    pthread_join(sensor_reader, NULL);
    pthread_join(console_reader, NULL);

    // Wait for all processes to finish
    for (int i = 0; i < config.n_workers + 1; i++) // +1 because of the alerts watcher
    {
        wait(NULL);
    }

    shmctl(shmid, IPC_RMID, NULL);

    return 0;
}

// Run: gcc -o SystemManager SystemManager.c SystemManagerFuncs.c SystemManager.h && ./SystemManager