#include "SystemManager.h"

bool process_command_worker(const char *buffer, int worker_id)
{
    if (strncmp(buffer, "stats", 5) == 0)
    {
        // Separate the command with format "stats console_id"
        char *temp = malloc(strlen(buffer));
        strcpy(temp, buffer);
        char *token = strtok(temp, " ");
        token = strtok(NULL, " ");
        int console_id = atoi(token);

        printf("Showing stats\n");
        sem_wait(mutex_shm);
        char *aux = get_key_list(&shm->key_list);

        message msg;
        msg.type = console_id;
        strcpy(msg.message, aux);

        if (msgsnd(msg_queue_id, &msg, sizeof(msg), 0) == -1)
        {
            perror("msgsnd");
            exit(1);
        }

        sem_post(mutex_shm);
    }
    else if (strncmp(buffer, "reset", 5) == 0)
    {
        // Separate the command with format "stats console_id"
        char *temp = malloc(strlen(buffer));
        strcpy(temp, buffer);
        char *token = strtok(temp, " ");
        token = strtok(NULL, " ");
        int console_id = atoi(token);

        printf("Worker %d: %s \n", worker_id, "RESET");
        sem_wait(mutex_shm);
        if (reset_keys(&shm->key_list) == false)
        {
            sem_post(mutex_shm);
            message msg;
            msg.type = console_id;
            strcpy(msg.message, "ERROR");
            msgsnd(msg_queue_id, &msg, sizeof(msg), 0);
        }
        else
        {
            sem_post(mutex_shm);
            message msg;
            msg.type = console_id;
            strcpy(msg.message, "OK");
            msgsnd(msg_queue_id, &msg, sizeof(msg), 0);
        }
    }
    else if (strncmp(buffer, "sensors", 7) == 0)
    {
        // Separate the command with format "stats console_id"
        char *temp = malloc(strlen(buffer));
        strcpy(temp, buffer);
        char *token = strtok(temp, " ");
        token = strtok(NULL, " ");
        int console_id = atoi(token);

        printf("Worker %d: %s \n", worker_id, "SENSORS");
        sem_wait(mutex_shm);
        char *aux = get_key_names(&shm->key_list);

        message msg;
        msg.type = console_id;
        strcpy(msg.message, aux);

        if (msgsnd(msg_queue_id, &msg, sizeof(msg), 0) == -1)
        {
            perror("msgsnd");
            exit(1);
        }

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
        token = strtok(NULL, " ");
        int console_id = atoi(token);

        // add_alert AL1 HOUSETEMP 10 25
        // add_alert AL1 ROOM1_TMP 11 24
        // remove_alert AL1
        // add_alert AL3 ROOM2_TMP 11 26

        struct alert_list_node alert = create_alert_list_node(id, key, min, max, console_id);
        sem_wait(mutex_shm);
        if (enqueue(&shm->alert_queue, alert) == false)
        {
            sem_post(mutex_shm);
            message msg;
            msg.type = console_id;
            strcpy(msg.message, "ERROR");
            msgsnd(msg_queue_id, &msg, sizeof(msg), 0);
        }
        else
        {
            sem_post(mutex_shm);
            message msg;
            msg.type = console_id;
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
        token = strtok(NULL, " ");
        int console_id = atoi(token);

        sem_wait(mutex_shm);
        if (dequeue_by_id(&shm->alert_queue, id) == false)
        {
            sem_post(mutex_shm);
            message msg;
            msg.type = console_id;
            strcpy(msg.message, "ERROR");
            msgsnd(msg_queue_id, &msg, sizeof(msg), 0);
        }
        else
        {
            sem_post(mutex_shm);
            message msg;
            msg.type = console_id;
            strcpy(msg.message, "OK");
            msgsnd(msg_queue_id, &msg, sizeof(msg), 0);
        }
    }
    else if (strncmp(buffer, "list_alerts", 11) == 0)
    {
        // Separate the command with format "stats console_id"
        char *temp = malloc(strlen(buffer));
        strcpy(temp, buffer);
        char *token = strtok(temp, " ");
        token = strtok(NULL, " ");
        int console_id = atoi(token);

        printf("Worker %d: %s \n", worker_id, "LIST_ALERTS");
        sem_wait(mutex_shm);
        char *aux = get_queue_list(&shm->alert_queue);

        message msg;
        msg.type = console_id;
        strcpy(msg.message, aux);

        if (msgsnd(msg_queue_id, &msg, sizeof(msg), 0) == -1)
        {
            perror("msgsnd");
            exit(1);
        }

        sem_post(mutex_shm);
    }
    else
    {
        return false;
    }

    return true;
}

Config read_config_file(char *filename)
{
    Config config;
    FILE *fp;
    int line_number = 1;

    fp = fopen(filename, "r");

    if (fp == NULL)
    {
        printf("Erro ao abrir o arquivo %s.\n", filename);
        exit(EXIT_FAILURE);
    }

    if (fscanf(fp, "%d\n%d\n%d\n%d\n%d", &config.queue_sz, &config.n_workers, &config.max_keys, &config.max_sensors, &config.max_alerts) != 5)
    {
        printf("Erro na linha %d: Arquivo de configuração inválido.\n", line_number);
        exit(EXIT_FAILURE);
    }

    if (config.queue_sz < 1)
    {
        printf("Erro na linha %d: QUEUE_SZ deve ser maior ou igual a 1.\n", line_number);
        exit(EXIT_FAILURE);
    }
    line_number++;

    if (config.n_workers < 1)
    {
        printf("Erro na linha %d: N_WORKERS deve ser maior ou igual a 1.\n", line_number);
        exit(EXIT_FAILURE);
    }
    line_number++;

    if (config.max_keys < 1)
    {
        printf("Erro na linha %d: MAX_KEYS deve ser maior ou igual a 1.\n", line_number);
        exit(EXIT_FAILURE);
    }
    line_number++;

    if (config.max_sensors < 1)
    {
        printf("Erro na linha %d: MAX_SENSORS deve ser maior ou igual a 1.\n", line_number);
        exit(EXIT_FAILURE);
    }
    line_number++;

    if (config.max_alerts < 0)
    {
        printf("Erro na linha %d: MAX_ALERTS deve ser maior ou igual a 0.\n", line_number);
        exit(EXIT_FAILURE);
    }

    fclose(fp);
    return config;
}

void print_config(Config config)
{
    printf("QUEUE_SZ: %d\nN_WORKERS: %d\nMAX_KEYS: %d\nMAX_SENSORS: %d\nMAX_ALERTS: %d\n", config.queue_sz, config.n_workers, config.max_keys, config.max_sensors, config.max_alerts);
}

void create_named_pipes()
{
    int result;

    // try to remove the named pipe in case it already exists
    unlink(CONSOLE_PIPE);
    unlink(SENSOR_PIPE);

    // Cria a named pipe CONSOL_PIPE com as permissões de leitura e escrita para todos
    result = mkfifo(CONSOLE_PIPE, 0777);
    if (result != 0)
    {
        perror("Erro ao criar a named pipe CONSOL_PIPE");
        exit(EXIT_FAILURE);
    }

    // Cria a named pipe SENSOR_PIPE com as permissões de leitura e escrita para todos
    result = mkfifo(SENSOR_PIPE, 0777);
    if (result != 0)
    {
        perror("Erro ao criar a named pipe SENSOR_PIPE.");
        exit(EXIT_FAILURE);
    }
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
        // check if key already exists in queue
        for (int i = q->front; i <= q->rear; i++)
        {
            if (strcmp(q->data[i].key, data.key) == 0)
            {
                printf("Key already exists in queue!\n");
                return false;
            }
        }

        q->rear++;
        if (q->rear == shm->config_file.max_alerts)
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

struct alert_list_node create_alert_list_node(char *id, char *key, int min_value, int max_value, int user_console_id)
{
    struct alert_list_node new_node;
    strcpy(new_node.id, id);
    strcpy(new_node.key, key);
    new_node.min_value = min_value;
    new_node.max_value = max_value;
    new_node.user_console_id = user_console_id;
    return new_node;
}

char *get_queue_list(struct queue *q)
{
    char *queue_list = malloc(BUFFER_SIZE);
    int position = 0;

    if (is_empty(q))
    {
        position += sprintf(queue_list + position, "Queue is empty!\n");
    }
    else
    {
        position += sprintf(queue_list + position, "%-10s %-15s %-10s %-10s\n", "ID", "Key", "MIN", "MAX");
        int i = q->front;
        while (i != q->rear)
        {
            position += sprintf(queue_list + position, "%-10s %-15s %-10d %-10d\n", q->data[i].id, q->data[i].key, q->data[i].min_value, q->data[i].max_value);
            i++;
            if (i == shm->config_file.max_alerts)
            {
                i = 0;
            }
        }
        position += sprintf(queue_list + position, "%-10s %-15s %-10d %-10d\n", q->data[i].id, q->data[i].key, q->data[i].min_value, q->data[i].max_value);
    }
    return queue_list;
}

int is_key_empty(struct key_queue *q)
{
    return (q->size == 0);
}

int is_key_full(struct key_queue *q)
{
    return (q->size == shm->config_file.max_keys);
}

void enqueue_key(struct key_queue *q, char *key, int value)
{
    // Verifica se a chave já existe na fila
    struct key_list_node *curr = q->data + q->front;
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

    // Se a chave não existe ainda, cria um novo nó de chave e adiciona à frente da fila
    if (is_key_full(q))
    {
        printf("Queue is full!\n");
        return;
    }
    else
    {
        q->front--;
        if (q->front < 0)
        {
            q->front = shm->config_file.max_keys - 1;
        }
        strcpy(q->data[q->front].key, key);
        q->data[q->front].last_value = value;
        q->data[q->front].min_value = value;
        q->data[q->front].max_value = value;
        q->data[q->front].avg_value = value;
        q->data[q->front].num_updates = 1;
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
        if (q->front == shm->config_file.max_keys)
        {
            q->front = 0;
        }
        q->size--;
        return temp_data;
    }
}

bool reset_keys(struct key_queue *q)
{
    if ((q->front == -1 && q->rear == -1) || q->size == 0)
    {
        // Fila vazia
        return false;
    }

    int i;
    for (i = q->front; i != q->rear + 1; i = (i + 1) % shm->config_file.max_keys)
    {
        q->data[i].last_value = 0;
        q->data[i].min_value = 0;
        q->data[i].max_value = 0;
        q->data[i].avg_value = 0;
        q->data[i].num_updates = 0;
    }

    return true;
}

char *get_key_list(struct key_queue *q)
{
    char *key_list = malloc(BUFFER_SIZE);
    int position = 0;

    position += sprintf(key_list + position, "%-10s %-10s %-10s %-10s %-10s %-10s\n", "Key", "Last", "Min", "Max", "Avg", "Count");

    if (q->size == 0)
    {
        position += sprintf(key_list + position, "Queue is empty.\n");
        return key_list;
    }

    struct key_list_node *curr = &q->data[q->front];
    for (int i = 0; i < q->size; i++)
    {
        position += sprintf(key_list + position, "%-10s %-10d %-10d %-10d %-10.2f %-10d\n", curr->key, curr->last_value, curr->min_value, curr->max_value, curr->avg_value, curr->num_updates);
        curr++;
        if (curr == &q->data[shm->config_file.max_keys])
        {
            curr = q->data;
        }
    }
    return key_list;
}

char *get_key_names(struct key_queue *q)
{
    char *key_names = malloc(BUFFER_SIZE);
    int position = 0;

    if (q->size == 0)
    {
        strcpy(key_names, "Queue is empty.");
        return key_names;
    }

    struct key_list_node *curr = &q->data[q->front];
    for (int i = 0; i < q->size; i++)
    {
        position += sprintf(key_names + position, "%s\n", curr->key);
        curr++;
        if (curr == &q->data[shm->config_file.max_keys])
        {
            curr = q->data;
        }
    }
    return key_names;
}

void init_key_queue(struct key_queue *q)
{
    q->front = -1;
    q->rear = -1;
    q->size = 0;
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

void terminate()
{
    write_log("\nHOME_IOT SIMULATOR TERMINATING");
    write_log("HOME_IOT SIMULATOR WAITING FOR LAST TASKS TO FINISH");
    write_log("HOME_IOT SIMULATOR CLOSING");

    shmctl(shmid, IPC_RMID, NULL);

    // unlink all the semaphores
    sem_unlink("mutex_shm");
    sem_unlink("log_sem");
    sem_unlink("internal_queue_sem");
    sem_unlink("worker_status_sem");

    // destroy all the semaphores
    sem_destroy(mutex_shm);
    sem_destroy(log_sem);
    sem_destroy(check_alert_sem);

    // destroy the pthread_mutex_t
    pthread_mutex_destroy(&internal_queue_mutex);

    // End the threads
    pthread_cancel(console_reader);
    pthread_cancel(sensor_reader);
    pthread_cancel(dispatcher);

    // Close the named pipes
    unlink(CONSOLE_PIPE);
    unlink(SENSOR_PIPE);

    // Remove a message queue at the end of the program
    msgctl(msg_queue_id, IPC_RMID, 0);

    fclose(log_file);

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
    init_key_queue(&shm->key_list);
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

void ignore_all_signals()
{
    int num;
    struct sigaction sa;
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    int i;
    for (i = 1; i <= 64; i++)
    {
        if (i == SIGINT || i == SIGTSTP)
        {
            num++;
            if (num == 111 || num == 112)
            {
                continue;
            }
            char *msg = malloc(100);
            sprintf(msg, "SIGNAL %s RECEIVED", strsignal(i));
            write_log(msg);
            free(msg);
            continue;
        }
        if (sigaction(i, &sa, NULL) == -1)
        {
            // Ignorar sinais que não são suportados
            if (errno == EINVAL || errno == ENOSYS)
            {
                continue;
            }
            perror("Erro ao definir acao do sinal");
            exit(EXIT_FAILURE);
        }
    }

    return;
}

// ############################################################################################################
