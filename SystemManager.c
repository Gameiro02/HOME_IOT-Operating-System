#include "SystemManager.h"
#include "log.h"

// Variaveis globais
int shmid;
SharedMemory *shm;
sem_t *mutex_shm;
sem_t *log_sem;
sem_t *internal_queue_sem;

struct InternalQueueNode *internal_queue;

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

void worker(int worker_id, int read_pipe, int write_pipe)
{
    char *message = malloc(100);
    sprintf(message, "WORKER %d CREATED", worker_id);
    sem_wait(log_sem);
    write_log(message);
    sem_post(log_sem);
    free(message);

    int read_bytes;
    char buffer[BUFFER_SIZE];

    while (1)
    {
        read_bytes = read(read_pipe, buffer, BUFFER_SIZE);

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
            printf("Worker %d: %s \n", worker_id, buffer);
        bzero(buffer, BUFFER_SIZE);
    }
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
        {
            printf("Sensor Reader Routine: %s \n", buffer);
            // sem_wait(internal_queue_sem);
            struct InternalQueueNode aux = parse_params(buffer);
            push_sensor_message_to_internal_queue(&internal_queue, aux.sensor_id, aux.key, aux.value, aux.command, aux.priority);
            // sem_post(internal_queue_sem);
        }
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
        {
            printf("User Console: %s \n", buffer);
            sem_wait(internal_queue_sem);
            push_sensor_message_to_internal_queue(&internal_queue, NULL, NULL, 0, buffer, 0);
            // print_internal_queue(internal_queue);
            sem_post(internal_queue_sem);
        }
        bzero(buffer, BUFFER_SIZE);
    }
    // Fecha a pipe e finaliza a thread
    close(fd);
    pthread_exit(NULL);
}

void alerts_watcher()
{
    sem_wait(log_sem);
    write_log("THREAD ALERTS_WATCHER CREATED");
    sem_post(log_sem);
}

void *dispatcher_routine(void *arg)
{
    sem_wait(log_sem);
    write_log("THREAD DISPATCHER CREATED");
    sem_post(log_sem);

    int(*pipes)[2] = (int(*)[2])arg;

    while (true)
    {
        if (internal_queue != NULL)
        {
            struct InternalQueueNode *node = pop(&internal_queue);
            // print_internal_queue(internal_queue);

            // Send the message to the worker 1 (use the ,macros defined in the header file: READ, WRITE)

            char *msg = create_msg_to_worker(node);
            printf("Sending message to worker 1: %s\n", msg);

            // try to send the message to the worker 1
            if (write(pipes[0][WRITE], msg, strlen(msg)) == -1)
            {
                perror("Error writing to pipe");
            }

            sleep(10);
        }
    }

    pthread_exit(NULL);
}

char *create_msg_to_worker(struct InternalQueueNode *node)
{
    char *msg = malloc(100);
    sprintf(msg, "%s#%s#%d", node->sensor_id, node->key, node->value);
    return msg;
}
struct InternalQueueNode parse_params(const char *str)
{
    struct InternalQueueNode params;

    char copy[256];
    strncpy(copy, str, sizeof(copy)); // cria uma cópia da string original para poder modificar

    char *token = strtok(copy, "#");
    if (token != NULL)
    {
        strncpy(params.sensor_id, token, sizeof(params.sensor_id));
        token = strtok(NULL, "#");
        if (token != NULL)
        {
            strncpy(params.key, token, sizeof(params.key));
            token = strtok(NULL, "#");
            if (token != NULL)
            {
                params.value = atoi(token);
            }
        }
    }

    return params;
}

bool push_sensor_message_to_internal_queue(struct InternalQueueNode **head, char *sensor, char *key, int value, char *command, int priority)
{
    // Aloca memória para o novo nó
    struct InternalQueueNode *newNode = (struct InternalQueueNode *)malloc(sizeof(struct InternalQueueNode));

    if (newNode == NULL)
    {
        // Se não houver memória disponível, retorna false
        return false;
    }

    // Atribui os valores aos campos do novo nó
    if (sensor != NULL)
        strcpy(newNode->sensor_id, sensor);
    if (key != NULL)
        strcpy(newNode->key, key);
    newNode->value = value;
    if (command != NULL)
        strcpy(newNode->command, command);
    newNode->next = NULL;

    // Se a lista estiver vazia, o novo nó será o primeiro
    if (*head == NULL)
    {
        *head = newNode;
    }
    else
    {
        // Procura a posição correta para inserir o novo nó
        struct InternalQueueNode *current = *head;
        struct InternalQueueNode *previous = NULL;

        while (current != NULL && current->priority <= priority)
        {
            previous = current;
            current = current->next;
        }

        // Insere o novo nó na posição correta
        if (previous == NULL)
        {
            newNode->next = *head;
            *head = newNode;
        }
        else
        {
            previous->next = newNode;
            newNode->next = current;
        }
    }

    return true;
}
struct InternalQueueNode *pop(struct InternalQueueNode **head)
{
    if (*head == NULL)
    {
        // Se a lista estiver vazia, retorna NULL
        return NULL;
    }
    else
    {
        // Armazena o primeiro nó da lista em uma variável temporária
        struct InternalQueueNode *temp = *head;
        // printa o sensor

        // Ajusta o ponteiro do primeiro nó para o próximo nó da lista
        *head = (*head)->next;

        // Retorna o nó removido
        return temp;
    }
}

void print_internal_queue(struct InternalQueueNode *head)
{
    printf("========== PRINT QUEUE ========== \n");
    while (head != NULL)
    {
        printf("Sensor: ");
        if (head->sensor_id != NULL)
        {
            printf("%s\n", head->sensor_id);
        }
        else
        {
            printf("NULL\n");
        }
        printf("Key: ");
        if (head->key != NULL)
        {
            printf("%s\n", head->key);
        }
        else
        {
            printf("NULL\n");
        }
        printf("Value: %d\n", head->value);
        printf("Command: ");
        if (head->command != NULL)
        {
            printf("%s\n", head->command);
        }
        else
        {
            printf("NULL\n");
        }
        printf("Priority: %d\n", head->priority);
        printf("------------------------\n");
        head = head->next;
    }
    printf("================================\n");
}

int main()
{
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
    log_sem = sem_open("log_sem", O_CREAT, 0777, 1);
    if (log_sem == SEM_FAILED)
    {
        perror("sem_open: ");
        exit(1);
    }

    sem_wait(log_sem);
    write_log("HOME_IOT SIMULATOR STARTING");
    sem_post(log_sem);

    // Create the INTERNAL_QUEUE sem
    internal_queue_sem = sem_open("internal_queue_sem", O_CREAT, 0777, 1);
    if (internal_queue_sem == SEM_FAILED)
    {
        perror("sem_open: ");
        exit(1);
    }

    inicilize_shared_memory(config);

    create_named_pipes();

    int pipes[config.n_workers][2];

    if (pipe(pipes[0]) == -1)
    {
        perror("unnamed pipe: ");
        exit(EXIT_FAILURE);
    }

    // Create pipes
    for (int i = 0; i < config.n_workers; i++)
    {
        if (pipe(pipes[i]) == -1)
        {
            perror("unnamed pipe: ");
            exit(EXIT_FAILURE);
        }
    }

    // Create workers
    for (int i = 0; i < config.n_workers; i++)
    {
        if (fork() == 0)
        {
            write(pipes[i][WRITE], "Hello", 6);
            worker(i, pipes[i][READ], pipes[i][WRITE]);
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
    pthread_create(&dispatcher, NULL, dispatcher_routine, pipes);

    pthread_join(sensor_reader, NULL);
    pthread_join(console_reader, NULL);
    pthread_join(dispatcher, NULL);

    // Wait for all processes to finish
    for (int i = 0; i < config.n_workers + 1; i++) // +1 because of the alerts watcher
    {
        wait(NULL);
    }

    shmctl(shmid, IPC_RMID, NULL);

    // unlink all the semaphores
    sem_unlink("mutex_shm");
    sem_unlink("log_sem");
    sem_unlink("internal_queue_sem");

    // destroy all the semaphores
    sem_destroy(mutex_shm);
    sem_destroy(log_sem);
    sem_destroy(internal_queue_sem);

    // Close the unnamed pipes
    for (int i = 0; i < config.n_workers; i++)
    {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }

    return 0;
}

// Run: gcc -o SystemManager SystemManager.c SystemManagerFuncs.c SystemManager.h log.c log.h && ./SystemManager