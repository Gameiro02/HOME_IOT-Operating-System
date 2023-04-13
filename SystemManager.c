// Gonçalo Neves - 2020239361
// André Carvalho -

#include "SystemManager.h"
#include "log.h"

// Variaveis globais
int shmid;
SharedMemory *shm;
sem_t *mutex_shm;
sem_t *log_sem;
sem_t *check_alert_sem;
FILE *log_file;

int msg_queue_id;

pthread_mutex_t internal_queue_mutex = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t cond_dispatcher = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_alerts_watcher = PTHREAD_COND_INITIALIZER;

struct InternalQueueNode *internal_queue;

bool check_alerts = true;

void worker(int worker_id, int read_pipe)
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

        // Imprime a mensagem
        if (read_bytes > 0)
        {
            printf("Worker %d: %s \n", worker_id, buffer);

            sem_wait(mutex_shm);
            shm->workers_status[worker_id] = 1;
            sem_post(mutex_shm);

            if (process_command_worker(buffer, worker_id))
            {
                bzero(buffer, BUFFER_SIZE);
                printf("Worker %d: %s \n", worker_id, "COMMAND DONE");
                sem_wait(mutex_shm);
                shm->workers_status[worker_id] = 0;
                sem_post(mutex_shm);
                continue;
            }

            if (!check_msg(buffer))
            {
                bzero(buffer, BUFFER_SIZE);
                printf("Worker %d: %s \n", worker_id, "INVALID MESSAGE");
                sem_wait(mutex_shm);
                shm->workers_status[worker_id] = 0;
                sem_post(mutex_shm);
                continue;
            }

            // Parse the message: sensor_id#key#value witout segmentation fault
            struct InternalQueueNode aux = parse_params(buffer);

            // Add the message to the internal queue
            enqueue_key(&shm->key_list, aux.key, aux.value);

            printf("Worker %d: %s \n", worker_id, "DONE1");

            sem_wait(mutex_shm);
            shm->workers_status[worker_id] = 0;
            sem_post(mutex_shm);

            sem_post(check_alert_sem);
            printf("Worker %d: %s \n", worker_id, "DONE2");
        }
        bzero(buffer, BUFFER_SIZE);
    }
}

void *sensor_reader_routine()
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
            printf("Sensor Reader Routine Received: %s \n", buffer);

            // sem_wait(internal_queue_sem);
            struct InternalQueueNode aux = parse_params(buffer);
            push_sensor_message_to_internal_queue(&internal_queue, aux.sensor_id, aux.key, aux.value, aux.command, aux.priority);
            // print_internal_queue(internal_queue);

            // sem_post(internal_queue_sem);
        }
        bzero(buffer, BUFFER_SIZE);
    }
    // Fecha a pipe e finaliza a thread
    close(fd);
    pthread_exit(NULL);
}

void *console_reader_routine()
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
            // sem_wait(internal_queue_sem);
            push_sensor_message_to_internal_queue(&internal_queue, NULL, NULL, 0, buffer, 0);
            // print_internal_queue(internal_queue);
            // sem_post(internal_queue_sem);
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
    write_log("PROCESS ALERTS_WATCHER CREATED");
    sem_post(log_sem);

    // Send hello message to console
    message msg;
    msg.type = WORKER_TO_CONSOLE;
    strcpy(msg.message, "HELLO FROM ALERTS_WATCHER");

    if (msgsnd(msg_queue_id, &msg, sizeof(msg), 0) == -1)
    {
        perror("Error: alerts_watcher: msgsnd");
        exit(EXIT_FAILURE);
    }

    while (check_alerts)
    {
        sem_wait(check_alert_sem);
        // sem_wait(mutex_shm);
        // printf("Alerts Watcher: Checking alerts\n");
        struct key_list_node *key_node = &shm->key_list.data[shm->key_list.front];
        char last_alert[BUFFER_SIZE] = "";
        int last_value = -1;
        for (int i = 0; i <= shm->key_list.size; i++)
        {
            int i = shm->key_list.front;
            // find corresponding alert in alert_queue
            while (i != shm->key_list.rear)
            {
                // printf("Alerts Watcher: Comparing %s with %s\n", shm->alert_queue.data[i].key, key_node->key);
                if (strcmp(shm->alert_queue.data[i].key, key_node->key) == 0)
                {
                    // check if last value is within alert limits
                    if (key_node->last_value < shm->alert_queue.data[i].min_value ||
                        key_node->last_value > shm->alert_queue.data[i].max_value)
                    {
                        // check if the current alert is the same as the last alert printed
                        if (strcmp(last_alert, key_node->key) != 0 ||
                            last_value != key_node->last_value)
                        {
                            // print alert and update last_alert
                            printf("ALERT: Key %s last value %d is outside alert limits [%d, %d]\n",
                                   key_node->key, key_node->last_value, shm->alert_queue.data[i].min_value, shm->alert_queue.data[i].max_value);
                            strcpy(last_alert, key_node->key);
                            last_value = key_node->last_value;

                            // Send alert message to console
                            message msg;
                            msg.type = WORKER_TO_CONSOLE;
                            // Example: ALERT AL1 (ROOM1_TEMP 10 TO 20) TRIGGERED
                            sprintf(msg.message, "ALERT %s (%s %d TO %d) TRIGGERED with value %d", shm->alert_queue.data[i].id, key_node->key, shm->alert_queue.data[i].min_value, shm->alert_queue.data[i].max_value, key_node->last_value);

                            if (msgsnd(msg_queue_id, &msg, sizeof(msg), 0) == -1)
                            {
                                perror("Error: alerts_watcher: msgsnd");
                                exit(EXIT_FAILURE);
                            }
                        }
                    }
                }

                i++;
                if (i == shm->config_file.max_alerts)
                {
                    i = 0;
                }
            }

            key_node++;
            if (key_node == &shm->key_list.data[shm->config_file.max_keys])
            {
                key_node = shm->key_list.data;
            }
        }
        // wait for some time before checking again
        // sem_post(mutex_shm);
    }
}

void *dispatcher_routine(void *arg)
{
    sem_wait(log_sem);
    write_log("THREAD DISPATCHER CREATED");
    sem_post(log_sem);

    int(*pipes)[2] = (int(*)[2])arg;

    sem_wait(mutex_shm);
    int num_workers = shm->config_file.n_workers;
    sem_post(mutex_shm);

    char msg[BUFFER_SIZE];

    while (true)
    {
        if (internal_queue != NULL)
        {
            // print_internal_queue(internal_queue);

            struct InternalQueueNode *node = pop(&internal_queue);
            if (node == NULL)
            {
                bzero(msg, BUFFER_SIZE);
                continue;
            }

            // Check it the messsage comes from the console or from a sensor
            if (is_user_command(node->command))
            {
                char *comando = malloc(100);
                sprintf(comando, "%s", node->command);
                strcpy(msg, comando);
            }
            else
            {
                char *sensor_msg = create_msg_to_worker(node);
                strcpy(msg, sensor_msg);
            }

            int random_worker;

            if (num_workers == 1)
            {
                random_worker = 0;
            }
            else
            {
                random_worker = rand() % num_workers;
            }
            // printf("aaa\n");
            sem_wait(mutex_shm);
            // printf("bbb\n");
            while (shm->workers_status[random_worker] != 0)
            {
                random_worker = rand() % num_workers;
            }
            sem_post(mutex_shm);

            // Send the message to the worker
            if (write(pipes[random_worker][WRITE], msg, strlen(msg)) == -1)
            {
                perror("Erro ao escrever na pipe");
                pthread_exit(NULL);
            }
            else
            {
                printf("Mensagem enviada para o worker %d: %s\n", random_worker, msg);
            }
            bzero(msg, BUFFER_SIZE);
        }
    }

    pthread_exit(NULL);
}

int main()
{
    clear_log();
    log_file = fopen("log.log", "a");

    Config config = read_config_file("config.txt");
    signal(SIGINT, terminate);

    // Create the Message Queue
    key_t key = ftok(".", QUEUE_KEY);
    msg_queue_id = msgget(key, IPC_CREAT | 0777);
    if (msg_queue_id < 0)
    {
        perror("msgget: ");
        terminate();
        exit(1);
    }

    // Create shared memory
    int shmid = shmget(IPC_PRIVATE, sizeof(SharedMemory), IPC_CREAT | 0777);
    if (shmid < 0)
    {
        perror("shmget: ");
        terminate();
        exit(1);
    }

    // Attach shared memory
    shm = (SharedMemory *)shmat(shmid, NULL, 0);

    if (shm == NULL)
    {
        perror("shmat: ");
        terminate();
        exit(1);
    }

    mutex_shm = sem_open("mutex_shm", O_CREAT, 0777, 1);
    if (mutex_shm == SEM_FAILED)
    {
        perror("sem_open: ");
        terminate();
        exit(1);
    }

    // Create the log sem
    log_sem = sem_open("log_sem", O_CREAT, 0777, 1);
    if (log_sem == SEM_FAILED)
    {
        perror("sem_open: ");
        terminate();
        exit(1);
    }

    check_alert_sem = sem_open("check_alert_sem", O_CREAT, 0777, 1);
    if (check_alert_sem == SEM_FAILED)
    {
        perror("sem_open: ");
        terminate();
        exit(1);
    }

    sem_wait(log_sem);
    write_log("HOME_IOT SIMULATOR STARTING");
    sem_post(log_sem);

    inicilize_shared_memory(config);

    create_named_pipes();

    int pipes[config.n_workers][2];

    if (pipe(pipes[0]) == -1)
    {
        perror("unnamed pipe: ");
        terminate();
        exit(EXIT_FAILURE);
    }

    // Create pipes
    for (int i = 0; i < config.n_workers; i++)
    {
        if (pipe(pipes[i]) == -1)
        {
            perror("unnamed pipe: ");
            terminate();
            exit(EXIT_FAILURE);
        }
    }

    // Create workers
    for (int i = 0; i < config.n_workers; i++)
    {
        pid_t pid = fork();
        if (pid == -1)
        {
            perror("fork: ");
            terminate();
            exit(EXIT_FAILURE);
        }
        else if (pid == 0)
        {
            worker(i, pipes[i][READ]);
            exit(0);
        }
    }

    // Create the Alerts Watcher process
    alerts_watcher_pid = fork();
    if (alerts_watcher_pid == -1)
    {
        perror("fork: ");
        terminate();
        exit(EXIT_FAILURE);
    }
    else if (alerts_watcher_pid == 0)
    {
        alerts_watcher();
        exit(0);
    }

    pthread_t console_reader, sensor_reader, dispatcher;

    if (pthread_create(&sensor_reader, NULL, sensor_reader_routine, NULL) != 0)
    {
        perror("pthread_create: ");
        terminate();
        exit(1);
    }

    if (pthread_create(&console_reader, NULL, console_reader_routine, NULL) != 0)
    {
        perror("pthread_create: ");
        terminate();
        exit(1);
    }

    if (pthread_create(&dispatcher, NULL, dispatcher_routine, pipes) != 0)
    {
        perror("pthread_create: ");
        terminate();
        exit(1);
    }

    if (pthread_join(sensor_reader, NULL) != 0)
    {
        perror("pthread_join: ");
        terminate();
        exit(1);
    }

    if (pthread_join(console_reader, NULL) != 0)
    {
        perror("pthread_join: ");
        terminate();
        exit(1);
    }

    if (pthread_join(dispatcher, NULL) != 0)
    {
        perror("pthread_join: ");
        terminate();
        exit(1);
    }

    // Wait for all processes to finish
    for (int i = 0; i < config.n_workers + 1; i++) // +1 because of the alerts watcher
    {
        wait(NULL);
    }

    return 0;
}

// Run: gcc -o SystemManager SystemManager.c SystemManagerFuncs.c SystemManager.h log.c log.h && ./SystemManager