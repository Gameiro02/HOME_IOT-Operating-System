#include "SystemManager.h"
#include "log.h"

// Variaveis globais
int shmid;
SharedMemory *shm;
sem_t *mutex_shm;
sem_t *log_sem;
sem_t *key_list_empty_sem;
sem_t *worker_status_sem;

int msg_queue_id;

pthread_mutex_t internal_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
struct InternalQueueNode *internal_queue;

void inicilize_shared_memory(Config config)
{
    sem_wait(mutex_shm);
    shm->config_file = config;
    shm->workers_status = malloc(sizeof(int) * config.n_workers);
    shm->num_keys_added = 0;
    shm->num_alerts_added = 0;

    for (int i = 0; i < config.n_workers; i++)
    {
        shm->workers_status[i] = 0;
    }
    init_queue(&shm->alert_queue);
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

            printf("Worker %d: %s \n", worker_id, "DONE2");
        }
        bzero(buffer, BUFFER_SIZE);
    }
}

bool process_command_worker(const char *buffer, int worker_id)
{
    if (strncmp(buffer, "stats", 5) == 0)
    {
        printf("Showing stats\n");
        sem_wait(mutex_shm);
        print_key_list(&shm->key_list);
        sem_post(mutex_shm);
    }
    else if (strncmp(buffer, "reset", 5) == 0)
    {
        printf("Worker %d: %s \n", worker_id, "RESET");
        sem_wait(mutex_shm);
        if (reset_keys(&shm->key_list) == false)
        {
            sem_post(mutex_shm);
            message msg;
            msg.type = WORKER_TO_CONSOLE;
            strcpy(msg.message, "ERROR");
            msgsnd(msg_queue_id, &msg, sizeof(msg), 0);
        }
        else
        {
            sem_post(mutex_shm);
            message msg;
            msg.type = WORKER_TO_CONSOLE;
            strcpy(msg.message, "OK");
            msgsnd(msg_queue_id, &msg, sizeof(msg), 0);
        }
    }
    else if (strncmp(buffer, "sensors", 7) == 0)
    {
        printf("Worker %d: %s \n", worker_id, "SENSORS");
        sem_wait(mutex_shm);
        print_key_names(&shm->key_list);
        sem_post(mutex_shm);
    }
    else if (strncmp(buffer, "add_alert", 9) == 0)
    {
        printf("Worker %d: %s \n", worker_id, "ADD_ALERT");

        // add_alert id key min max
        char *aux = malloc(strlen(buffer));
        strcpy(aux, buffer);
        char *token = strtok(aux, " ");
        token = strtok(NULL, " ");
        char *id = malloc(strlen(token));
        strcpy(id, token);
        token = strtok(NULL, " ");
        char *key = malloc(strlen(token));
        strcpy(key, token);
        token = strtok(NULL, " ");
        int min = atoi(token);
        token = strtok(NULL, " ");
        int max = atoi(token);

        // add_alert AL1 ROOM1_TMP 10 25
        // add_alert AL2 ROOM1_TMP 10 25
        // add_alert AL3 ROOM2_TMP 11 26

        struct alert_list_node alert = create_alert_list_node(id, key, min, max);
        sem_wait(mutex_shm);
        if (enqueue(&shm->alert_queue, alert) == false)
        {
            sem_post(mutex_shm);
            message msg;
            msg.type = WORKER_TO_CONSOLE;
            strcpy(msg.message, "ERROR");
            msgsnd(msg_queue_id, &msg, sizeof(msg), 0);
        }
        else
        {
            sem_post(mutex_shm);
            message msg;
            msg.type = WORKER_TO_CONSOLE;
            strcpy(msg.message, "OK");
            msgsnd(msg_queue_id, &msg, sizeof(msg), 0);
        }
    }
    else if (strncmp(buffer, "remove_alert", 12) == 0)
    {
        printf("Worker %d: %s \n", worker_id, "REMOVE_ALERT");
        char *aux = malloc(strlen(buffer));
        strcpy(aux, buffer);
        char *token = strtok(aux, " ");
        token = strtok(NULL, " ");
        char *id = malloc(strlen(token));
        strcpy(id, token);

        sem_wait(mutex_shm);
        if (dequeue_by_id(&shm->alert_queue, id) == false)
        {
            sem_post(mutex_shm);
            message msg;
            msg.type = WORKER_TO_CONSOLE;
            strcpy(msg.message, "ERROR");
            msgsnd(msg_queue_id, &msg, sizeof(msg), 0);
        }
        else
        {
            sem_post(mutex_shm);
            message msg;
            msg.type = WORKER_TO_CONSOLE;
            strcpy(msg.message, "OK");
            msgsnd(msg_queue_id, &msg, sizeof(msg), 0);
        }
    }
    else if (strncmp(buffer, "list_alerts", 11) == 0)
    {
        printf("Worker %d: %s \n", worker_id, "LIST_ALERTS");
        sem_wait(mutex_shm);
        print_queue(&shm->alert_queue);
        sem_post(mutex_shm);
    }
    else
    {
        return false;
    }

    return true;
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
}

bool is_user_command(char *msg)
{
    if (msg == NULL)
    {
        printf("Error: is_user_command: msg is NULL\n");
        return false;
    }

    if (strncmp(msg, "exit", 4) == 0)
    {
        return true;
    }
    else if (strncmp(msg, "stats", 5) == 0)
    {
        return true;
    }
    else if (strncmp(msg, "reset", 5) == 0)
    {
        return true;
    }
    else if (strncmp(msg, "sensors", 7) == 0)
    {
        return true;
    }
    else if (strncmp(msg, "add_alert", 9) == 0)
    {
        return true;
    }
    else if (strncmp(msg, "remove_alert", 12) == 0)
    {
        return true;
    }
    else if (strncmp(msg, "list_alerts", 11) == 0)
    {
        return true;
    }
    else
    {
        return false;
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

    pthread_mutex_lock(&internal_queue_mutex);

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

    pthread_mutex_unlock(&internal_queue_mutex);
    // print_internal_queue(*head);
    return true;
}
struct InternalQueueNode *pop(struct InternalQueueNode **head)
{
    pthread_mutex_lock(&internal_queue_mutex);
    if (*head == NULL)
    {
        // Se a lista estiver vazia, retorna NULL
        pthread_mutex_unlock(&internal_queue_mutex);
        return NULL;
    }
    else
    {
        // Armazena o primeiro nó da lista em uma variável temporária
        struct InternalQueueNode *temp = *head;

        // Ajusta o ponteiro do primeiro nó para o próximo nó da lista
        *head = (*head)->next;

        // Retorna o nó removido
        pthread_mutex_unlock(&internal_queue_mutex);
        return temp;
    }
}

void print_internal_queue(struct InternalQueueNode *head)
{
    pthread_mutex_lock(&internal_queue_mutex);
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
    pthread_mutex_unlock(&internal_queue_mutex);
}

bool add_or_update_node(struct key_list_node **head, char *key, int value)
{
    if (head == NULL || key == NULL || mutex_shm == NULL)
    {
        return false;
    }

    sem_wait(mutex_shm);

    // Check if the max number of keys has been reached
    if (shm->num_keys_added >= shm->config_file.max_keys)
    {
        write_log("Max keys reached. Ignoring new key.");
        sem_post(mutex_shm);
        return false;
    }

    struct key_list_node *curr = *head;
    struct key_list_node *prev = NULL;
    while (curr != NULL)
    {
        if (strcmp(curr->key, key) == 0)
        {
            // Se a chave já existe, atualize as informações do nó
            curr->last_value = value;
            if (value < curr->min_value)
            {
                curr->min_value = value;
            }
            if (value > curr->max_value)
            {
                curr->max_value = value;
            }
            curr->avg_value = (curr->avg_value * curr->num_updates + value) / (curr->num_updates + 1);
            curr->num_updates++;
            sem_post(mutex_shm);
            return true;
        }
        prev = curr;
        curr = curr->next;
    }

    // Se a chave não existe, crie um novo nó e adicione-o ao final da lista
    struct key_list_node *new_node = (struct key_list_node *)malloc(sizeof(struct key_list_node));
    if (new_node == NULL)
    {
        sem_post(mutex_shm);
        return false;
    }
    strcpy(new_node->key, key);
    new_node->last_value = value;
    new_node->min_value = value;
    new_node->max_value = value;
    new_node->avg_value = value;
    new_node->num_updates = 1;
    new_node->next = NULL;

    if (*head == NULL)
    {
        *head = new_node;
    }
    else
    {
        prev->next = new_node;
    }

    sem_post(mutex_shm);
    return true;
}

void handle_sigint(int sig)
{
    printf("Received SIGINT signal. Exiting...\n");

    shmctl(shmid, IPC_RMID, NULL);

    // unlink all the semaphores
    sem_unlink("mutex_shm");
    sem_unlink("log_sem");
    sem_unlink("internal_queue_sem");
    sem_unlink("worker_status_sem");

    // destroy all the semaphores
    sem_destroy(mutex_shm);
    sem_destroy(log_sem);
    sem_destroy(worker_status_sem);

    // Close the named pipes
    unlink(CONSOLE_PIPE);
    unlink(SENSOR_PIPE);

    // Remove a message queue at the end of the program
    msgctl(msg_queue_id, IPC_RMID, 0);

    exit(0);
}

bool check_msg(char *buffer)

{
    // Verifica se a string é nula ou vazia
    if (buffer == NULL || buffer[0] == '\0')
    {
        return false;
    }

    // Copia a string para uma variável temporária
    char str[100];
    strcpy(str, buffer);

    char *token = strtok(str, "#");
    int count = 0;

    while (token != NULL)
    {
        count++;
        if (count == 1)
        {
            // Verifica o sensor_id
            if (strlen(token) < 1)
            {
                return false;
            }
        }
        else if (count == 2)
        {
            // Verifica a key
            if (strlen(token) < 1)
            {
                return false;
            }
        }
        else if (count == 3)
        {
            // Verifica o value
            int val = atoi(token);
            if (val <= 0)
            {
                return false;
            }
        }
        else
        {
            // Formato inválido
            return false;
        }
        token = strtok(NULL, "#");
    }

    // Verifica se há exatamente três campos separados por '#'
    if (count != 3)
    {
        return false;
    }

    return true;
}

int main()
{
    clear_log();
    Config config = read_config_file("config.txt");
    signal(SIGINT, handle_sigint);

    // Create the Message Queue
    key_t key = ftok(".", QUEUE_KEY);
    msg_queue_id = msgget(key, IPC_CREAT | 0777);
    if (msg_queue_id < 0)
    {
        perror("msgget: ");
        exit(1);
    }

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

    // Create the worker status sem
    worker_status_sem = sem_open("worker_status_sem", O_CREAT, 0777, 1);
    if (worker_status_sem == SEM_FAILED)
    {
        perror("sem_open: ");
        exit(1);
    }

    key_list_empty_sem = sem_open("key_list_empty_sem", O_CREAT, 0777, 1);
    if (key_list_empty_sem == SEM_FAILED)
    {
        perror("sem_open: ");
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
            // write(pipes[i][WRITE], "Hello", 6);
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

    return 0;
}

void init_queue(struct queue *q)
{
    q->front = 0;
    q->rear = -1;
    q->size = 0;
}

int is_empty(struct queue *q)
{
    return (q->size == 0);
}

int is_full(struct queue *q)
{
    return (q->size == shm->config_file.max_alerts);
}

bool enqueue(struct queue *q, struct alert_list_node data)
{
    if (is_full(q))
    {
        printf("Queue is full!\n");
        return false;
    }
    else
    {
        q->rear++;
        if (q->rear == QUEUE_SIZE)
        {
            q->rear = 0;
        }
        q->data[q->rear] = data;
        q->size++;
        return true;
    }
}

bool dequeue_by_id(struct queue *q, char *id)
{
    if (is_empty(q))
    {
        printf("Queue is empty!\n");
        return false;
    }
    else
    {
        int found = 0;
        int index = q->front;
        while (index <= q->rear)
        {
            if (strcmp(q->data[index].id, id) == 0)
            {
                found = 1;
                break;
            }
            index++;
        }
        if (found == 0)
        {
            return false;
        }
        else
        {
            while (index > q->front)
            {
                q->data[index] = q->data[index - 1];
                index--;
            }
            q->front++;
            q->size--;
        }
    }
    return true;
}

struct alert_list_node create_alert_list_node(char *id, char *key, int min_value, int max_value)
{
    struct alert_list_node new_node;
    strcpy(new_node.id, id);
    strcpy(new_node.key, key);
    new_node.min_value = min_value;
    new_node.max_value = max_value;
    return new_node;
}

void print_queue(struct queue *q)
{
    if (is_empty(q))
    {
        printf("Queue is empty!\n");
    }
    else
    {
        printf("%-10s %-15s %-10s %-10s\n", "ID", "Key", "MIN", "MAX");
        int i = q->front;
        while (i != q->rear)
        {
            printf("%-10s %-15s %-10d %-10d\n", q->data[i].id, q->data[i].key, q->data[i].min_value, q->data[i].max_value);
            i++;
            if (i == QUEUE_SIZE)
            {
                i = 0;
            }
        }
        printf("%-10s %-15s %-10d %-10d\n", q->data[i].id, q->data[i].key, q->data[i].min_value, q->data[i].max_value);
    }
}

int is_key_empty(struct key_queue *q)
{
    return (q->size == 0);
}

int is_key_full(struct key_queue *q)
{
    return (q->size == QUEUE_SIZE);
}

void enqueue_key(struct key_queue *q, char *key, int value)
{
    // Verifica se a chave já existe na fila
    struct key_list_node *curr = q->data;
    for (int i = 0; i < q->size; i++)
    {
        if (strcmp(curr->key, key) == 0)
        {
            // Se a chave já existe, atualiza os dados correspondentes
            if (value < curr->min_value)
            {
                curr->min_value = value;
            }
            if (value > curr->max_value)
            {
                curr->max_value = value;
            }
            curr->avg_value = (curr->avg_value * curr->num_updates + value) / (curr->num_updates + 1);
            curr->last_value = value;
            curr->num_updates++;
            return;
        }
        curr++;
    }

    // Se a chave não existe ainda, cria um novo nó de chave e adiciona à fila
    if (is_key_full(q))
    {
        printf("Queue is full!\n");
        exit(1);
    }
    else
    {
        q->rear++;
        if (q->rear == QUEUE_SIZE)
        {
            q->rear = 0;
        }
        strcpy(q->data[q->rear].key, key);
        q->data[q->rear].last_value = value;
        q->data[q->rear].min_value = value;
        q->data[q->rear].max_value = value;
        q->data[q->rear].avg_value = value;
        q->data[q->rear].num_updates = 1;
        q->data[q->rear].next = NULL;
        q->size++;
    }
}
struct key_list_node dequeue_key(struct key_queue *q)
{
    if (is_key_empty(q))
    {
        printf("Queue is empty!\n");
        exit(1);
    }
    else
    {
        struct key_list_node temp_data = q->data[q->front];
        q->front++;
        if (q->front == QUEUE_SIZE)
        {
            q->front = 0;
        }
        q->size--;
        return temp_data;
    }
}

bool reset_keys(struct key_queue *q)
{
    if (q->front == -1 && q->rear == -1 || q->size == 0)
    {
        // Fila vazia
        return false;
    }

    int i;
    for (i = q->front; i != q->rear + 1; i = (i + 1) % QUEUE_SIZE)
    {
        q->data[i].last_value = 0;
        q->data[i].min_value = 0;
        q->data[i].max_value = 0;
        q->data[i].avg_value = 0;
        q->data[i].num_updates = 0;
    }

    return true;
}

void print_key_list(struct key_queue *q)
{
    printf("Key\tLast\tMin\tMax\tAvg\tCount\n");

    // Percorre a fila de chaves e imprime as informações no formato solicitado
    struct key_list_node *curr = q->data;
    for (int i = 0; i < q->size; i++)
    {
        printf("%s\t%d\t%d\t%d\t%.2f\t%d\n",
               curr->key,        // chave
               curr->last_value, // último valor
               curr->min_value,  // valor mínimo
               curr->max_value,  // valor máximo
               curr->avg_value,  // valor médio
               curr->num_updates // número de atualizações
        );
        curr++;
    }
}

void print_key_names(struct key_queue *q)
{
    // Percorre a fila de chaves e imprime os nomes das chaves em linhas separadas
    struct key_list_node *curr = q->data;
    for (int i = 0; i < q->size; i++)
    {
        printf("%s\n", curr->key);
        curr++;
    }
}

// Run: gcc -o SystemManager SystemManager.c SystemManagerFuncs.c SystemManager.h log.c log.h && ./SystemManager