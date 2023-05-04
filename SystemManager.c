// Gonçalo Neves  - 2020239361
// André Carvalho - 2020237655

#include "SystemManager.h"
#include "log.h"

// Variaveis globais
int shmid;
SharedMemory *shm;
sem_t *mutex_shm;
sem_t *log_sem;
sem_t *check_alert_sem;
FILE *log_file;
pthread_t console_reader, sensor_reader, dispatcher;
int msg_queue_id;
pthread_mutex_t internal_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t internal_queue_cond = PTHREAD_COND_INITIALIZER;

struct InternalQueueNode *internal_queue;

bool check_alerts = true;
bool debug = false;

void worker(int worker_id, int read_pipe)
{
    char *message = malloc(100);
    sprintf(message, "WORKER %d READY", worker_id);
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
            if (debug)
                printf("Worker %d: %s \n", worker_id, buffer);

            sem_wait(mutex_shm);
            shm->workers_status[worker_id] = 1;
            sem_post(mutex_shm);

            char *message = malloc(1024);
            if (process_command_worker(buffer, worker_id, message))
            {
                char *message_aux = malloc(1024);
                sprintf(message_aux, "WORKER %d: %s %s", worker_id, buffer, "PROCESSING COMPLETED");

                sem_wait(log_sem);
                write_log(message_aux);
                sem_post(log_sem);

                free(message_aux);

                bzero(buffer, BUFFER_SIZE);
                sem_wait(mutex_shm);
                shm->workers_status[worker_id] = 0;
                sem_post(mutex_shm);
                continue;
            }

            if (!check_msg(buffer))
            {
                bzero(buffer, BUFFER_SIZE);
                // printf("Worker %d: %s \n", worker_id, "WRONG COMMAND");
                char *message_aux = malloc(1024);
                sprintf(message_aux, "WORKER %d: %s", worker_id, "WRONG COMMAND");

                sem_wait(log_sem);
                write_log(message_aux);
                sem_post(log_sem);

                free(message_aux);

                sem_wait(mutex_shm);
                shm->workers_status[worker_id] = 0;
                sem_post(mutex_shm);
                continue;
            }

            // Parse the message: sensor_id#key#value witout segmentation fault
            struct InternalQueueNode aux = parse_params(buffer);

            // Add the message to the internal queue
            sem_wait(mutex_shm);
            enqueue_key(&shm->key_list, aux.key, aux.value);
            sem_post(mutex_shm);

            if (debug)
                printf("Worker %d: %s \n", worker_id, "DONE1");

            sem_wait(mutex_shm);
            shm->workers_status[worker_id] = 0;
            sem_post(mutex_shm);

            sem_post(check_alert_sem);

            // If its a sensor message print WORKER<num_worker>: <sensor key> DATA PROCESSING COMPLETED
            if (aux.sensor_id != NULL)
            {
                char *message = malloc(100);
                sprintf(message, "WORKER %d: %s DATA PROCESSING COMPLETED", worker_id, aux.key);
                sem_wait(log_sem);
                write_log(message);
                sem_post(log_sem);
                free(message);
            }
        }
        bzero(buffer, BUFFER_SIZE);
    }
}

void *sensor_reader_routine()
{
    sem_wait(log_sem);
    write_log("THREAD SENSOR_READER CREATED");
    sem_post(log_sem);

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
            if (debug)
                printf("Sensor Reader Routine Received: %s \n", buffer);
            struct InternalQueueNode aux = parse_params(buffer);
            push_sensor_message_to_internal_queue(&internal_queue, aux.sensor_id, aux.key, aux.value, aux.command, aux.priority);

            pthread_mutex_lock(&internal_queue_mutex);
            pthread_cond_signal(&internal_queue_cond);
            pthread_mutex_unlock(&internal_queue_mutex);
        }
        bzero(buffer, BUFFER_SIZE);
    }
    // Fecha a pipe e finaliza a thread
    close(fd);
    pthread_exit(NULL);
}

void *console_reader_routine()
{
    sem_wait(log_sem);
    write_log("THREAD CONSOLE_READER CREATED");
    sem_post(log_sem);

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
            if (debug)
                printf("User Console: %s \n", buffer);
            push_sensor_message_to_internal_queue(&internal_queue, NULL, NULL, 0, buffer, 0);

            // Signal the internal queue that there is a new message (condition variable is now open)
            pthread_mutex_lock(&internal_queue_mutex);
            pthread_cond_signal(&internal_queue_cond);
            pthread_mutex_unlock(&internal_queue_mutex);
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

    while (check_alerts)
    {
        sem_wait(check_alert_sem);

        // printf("Alerts Watcher: Checking alerts\n");
        char last_alert[BUFFER_SIZE] = "";

        sem_wait(mutex_shm);
        struct key_list_node *key_node = &shm->key_list.data[shm->key_list.front];
        sem_post(mutex_shm);

        for (int i = 0; i <= shm->key_list.size; i++)
        {
            sem_wait(mutex_shm);
            int i = shm->key_list.front;
            int last = shm->key_list.rear;

            // Save the first node
            struct alert_list_node first_node = shm->alert_queue.data[shm->alert_queue.front];

            // If the node we are in is the first node, we break the loop
            // if (strcmp(shm->alert_queue.data[i].key, key_node->key) == 0)
            // {
            //     printf("WE BREAKED\n");
            //     sem_post(mutex_shm);
            //     break;
            // }

            sem_post(mutex_shm);
            // find corresponding alert in alert_queue
            while (i != last)
            {
                sem_wait(mutex_shm);

                // printf("Alerts Watcher: Comparing %s with %s\n", shm->alert_queue.data[i].key, key_node->key);
                if (strcmp(shm->alert_queue.data[i].key, key_node->key) == 0)
                {
                    // check if last value is within alert limits
                    if (key_node->last_value < shm->alert_queue.data[i].min_value ||
                        key_node->last_value > shm->alert_queue.data[i].max_value)
                    {
                        // check if the current alert is the same as the last alert printed
                        if (strcmp(last_alert, key_node->key) != 0)
                        {
                            // print alert and update last_alert
                            if (debug)
                                printf("ALERT: Key %s last value %d is outside alert limits [%d, %d]\n",
                                       key_node->key, key_node->last_value, shm->alert_queue.data[i].min_value, shm->alert_queue.data[i].max_value);
                            strcpy(last_alert, key_node->key);

                            // Send alert message to console
                            message msg;
                            msg.type = shm->alert_queue.data[i].user_console_id;
                            // Example: ALERT AL1 (ROOM1_TEMP 10 TO 20) TRIGGERED
                            sprintf(msg.message, "ALERT %s (%s %d TO %d) TRIGGERED WITH VALUE %d", shm->alert_queue.data[i].id, key_node->key, shm->alert_queue.data[i].min_value, shm->alert_queue.data[i].max_value, key_node->last_value);
                            sem_wait(log_sem);
                            write_log(msg.message);
                            sem_post(log_sem);
                            if (msgsnd(msg_queue_id, &msg, sizeof(msg) - sizeof(long), 0) == -1)
                            {
                                perror("Error: alerts_watcher: msgsnd");
                                exit(EXIT_FAILURE);
                            }
                        }
                    }
                }
                // If the node we are in is the first node, we break the loop
                if (strcmp(shm->alert_queue.data[i].key, first_node.key) == 0)
                {
                    if (debug)
                        printf("WE BREAKED2\n");
                    sem_post(mutex_shm);
                    break;
                }

                i++;
                if (i == shm->config_file.max_alerts)
                {
                    i = 0;
                }
                sem_post(mutex_shm);
            }
            sem_wait(mutex_shm);
            key_node++;
            if (key_node == &shm->key_list.data[shm->config_file.max_keys])
            {
                key_node = shm->key_list.data;
            }
            sem_post(mutex_shm);
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
        // wait for the internal queue to have at least one node
        pthread_mutex_lock(&internal_queue_mutex);
        while (internal_queue == NULL)
        {
            pthread_cond_wait(&internal_queue_cond, &internal_queue_mutex);
        }
        pthread_mutex_unlock(&internal_queue_mutex);

        // pop a node from the internal queue
        struct InternalQueueNode *node = pop(&internal_queue);
        if (node == NULL)
        {
            bzero(msg, BUFFER_SIZE);
            continue;
        }

        // Check if the message comes from the console or from a sensor
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

        // choose a random worker to send the message to
        int random_worker;
        if (num_workers == 1)
        {
            random_worker = 0;
        }
        else
        {
            random_worker = rand() % num_workers;
        }
        sem_wait(mutex_shm);
        while (shm->workers_status[random_worker] != 0)
        {
            random_worker = rand() % num_workers;
        }
        sem_post(mutex_shm);

        // send the message to the worker
        if (write(pipes[random_worker][WRITE], msg, strlen(msg)) == -1)
        {
            perror("Erro ao escrever na pipe");
            pthread_exit(NULL);
        }
        else
        {
            if (debug)
                printf("Mensagem enviada para o worker %d: %s\n", random_worker, msg);

            // if the message is a sensor msg write to log a msg with this format: "DISPATCHER: ROOM1_TEMP DATA (FROM ROOM1T1 SENSOR) SENT FOR PROCESSING ON WORKER 1"
            // else if the message is a user command write to log a msg with this format: "DISPATCHER: ADD ALERT AL1 (ROOM1_TEMP 10 TO 20) SENT FOR PROCESSING ON WORKER 2"

            char *log_msg = malloc(1024);

            if (node->command != NULL && strcmp(node->command, "") != 0)
            {
                char *command_aux = malloc(1024);

                // put the command in the command_aux in UPPER CASE and dont put the user_console_id in the command_aux
                for (int i = 0; i < (int)strlen(node->command); i++)
                {
                    command_aux[i] = toupper(node->command[i]);
                }

                sprintf(log_msg, "DISPATCHER: %s SENT FOR PROCESSING ON WORKER %d", command_aux, random_worker);
            }
            else if (node->key != NULL)
            {
                sprintf(log_msg, "DISPATCHER: %s DATA (FROM %s SENSOR) SENT FOR PROCESSING ON WORKER %d", node->key, node->sensor_id, random_worker);
            }

            sem_wait(log_sem);
            write_log(log_msg);
            sem_post(log_sem);
            free(log_msg);
        }
        bzero(msg, BUFFER_SIZE);
    }

    pthread_exit(NULL);
}

int main()
{
    clear_log();
    log_file = fopen("log.log", "a");

    Config config = read_config_file("config.txt");

    printf("Todos os sinais, exceto SIGINT e SIGTSTP, serao ignorados.\n");

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
    int shmid = shmget(IPC_PRIVATE, sizeof(SharedMemory) + sizeof(struct alert_list_node) * config.max_alerts + sizeof(struct key_list_node) * config.max_keys + sizeof(int) * config.n_workers, IPC_CREAT | 0777);
    // int shmid = shmget(IPC_PRIVATE, sizeof(SharedMemory), IPC_CREAT | 0777);
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
    pid_t pid = fork();
    if (pid == -1)
    {
        perror("fork: ");
        terminate();
        exit(EXIT_FAILURE);
    }
    else if (pid == 0)
    {
        alerts_watcher();
        exit(0);
    }

    signal(SIGINT, terminate);
    signal(SIGTSTP, handle_sigstp);
    ignore_all_signals();

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