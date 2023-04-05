#include "SystemManager.h"
#include "log.h"

// Variaveis globais
int shmid;
SharedMemory *shm;
sem_t *mutex_shm;
sem_t *log_sem;
sem_t *key_list_empty_sem;
sem_t *worker_status_sem;

pthread_mutex_t internal_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
struct InternalQueueNode *internal_queue;
struct key_list_node key_list_head;
struct alert_list_node alert_list_head;

void inicilize_shared_memory(Config config)
{
    sem_wait(mutex_shm);
    shm->config_file = config;
    shm->workers_status = malloc(sizeof(int) * config.n_workers);
    shm->key_list = NULL;
    shm->num_keys_added = 0;
    shm->num_alerts_added = 0;

    for (int i = 0; i < config.n_workers; i++)
    {
        shm->workers_status[i] = 0;
    }
    shm->key_list = &key_list_head;
    shm->alert_list = &alert_list_head;
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

            if (!add_or_update_node(&shm->key_list, aux.key, aux.value))
            {
                bzero(buffer, BUFFER_SIZE);
                printf("Worker %d: %s \n", worker_id, "ERROR UPDATING KEY LIST");
                sem_wait(mutex_shm);
                shm->workers_status[worker_id] = 0;
                sem_post(mutex_shm);
            }

            printf("Worker %d: %s \n", worker_id, "DONE1");

            sem_wait(mutex_shm);
            shm->workers_status[worker_id] = 0;
            sem_post(mutex_shm);

            printf("Worker %d: %s \n", worker_id, "DONE2");
        }
        bzero(buffer, BUFFER_SIZE);
    }
}

void print_only_the_keys()
{
    sem_wait(mutex_shm);
    struct key_list_node *aux = shm->key_list;
    while (aux != NULL)
    {
        printf("%s\n", aux->key);
        aux = aux->next;
    }
    sem_post(mutex_shm);
}

bool process_command_worker(const char *buffer, int worker_id)
{
    if (strncmp(buffer, "stats", 5) == 0)
    {
        printf("Showing stats\n");
        print_key_list_to_user();
    }
    else if (strncmp(buffer, "reset", 5) == 0)
    {
        printf("Worker %d: %s \n", worker_id, "RESET");
        reset_key_list();
        printf("OK\n");
    }
    else if (strncmp(buffer, "sensors", 7) == 0)
    {
        printf("Worker %d: %s \n", worker_id, "SENSORS");
        print_only_the_keys();
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

        add_alert(&shm->alert_list, id, key, min, max);
        // list_alerts(shm->alert_list);
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
        remove_alert(&shm->alert_list, id);
    }
    else if (strncmp(buffer, "list_alerts", 11) == 0)
    {
        printf("Worker %d: %s \n", worker_id, "LIST_ALERTS");
        list_alerts(shm->alert_list);
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

void print_key_list_to_user()
{
    sem_wait(mutex_shm);

    if (shm->key_list == NULL)
    {
        printf("No keys to show\n");
        sem_post(mutex_shm);
        return;
    }

    struct key_list_node *head = shm->key_list;
    printf("%-20s%-10s%-10s%-10s%-10s%-10s\n", "Key", "Last", "Min", "Max", "Avg", "Count");
    while (head != NULL)
    {
        printf("%-20s", head->key);
        printf("%-10d", head->last_value);
        printf("%-10d", head->min_value);
        printf("%-10d", head->max_value);
        printf("%-10.2f", head->avg_value);
        printf("%-10d\n", head->num_updates);
        head = head->next;
    }
    sem_post(mutex_shm);
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

void reset_key_list()
{
    sem_wait(mutex_shm);
    struct key_list_node *head = shm->key_list;
    while (head != NULL)
    {
        head->last_value = 0;
        head->min_value = 0;
        head->max_value = 0;
        head->avg_value = 0;
        head->num_updates = 0;
        head = head->next;
    }
    sem_post(mutex_shm);
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

void push_key_list(struct key_list_node **head, char *key, int value)
{
    struct key_list_node *newNode = (struct key_list_node *)malloc(sizeof(struct key_list_node));

    sem_wait(mutex_shm);
    if (shm->num_keys_added >= shm->config_file.max_keys)
    {
        write_log("Max keys reached. Ignoring new key.");
        sem_post(mutex_shm);
        return;
    }

    if (newNode == NULL)
    {
        sem_post(mutex_shm);
        return;
    }

    if (key != NULL)
    {
        strcpy(newNode->key, key);
    }
    else
    {
        strcpy(newNode->key, "null");
    }

    newNode->last_value = value;
    newNode->min_value = value;
    newNode->max_value = value;
    newNode->avg_value = value;
    newNode->num_updates = 1;
    newNode->next = NULL;

    if (*head == NULL)
    {
        *head = newNode;
    }
    else
    {
        struct key_list_node *current = *head;
        struct key_list_node *previous = NULL;

        while (current != NULL && strcmp(current->key, key) != 0)
        {
            previous = current;
            current = current->next;
        }

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
    // print_key_list(*head);
    shm->num_keys_added++;
    sem_post(mutex_shm);
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

void print_key_list()
{
    sem_wait(mutex_shm);
    struct key_list_node *head = shm->key_list;

    printf("========== PRINT KEY LIST ========== \n");
    while (head != NULL)
    {
        printf("Key: ");
        if (head->key != NULL)
        {
            printf("%s\n", head->key);
        }
        else
        {
            printf("NULL\n");
        }
        printf("Last Value: %d\n", head->last_value);
        printf("Avg Value: %f\n", head->avg_value);
        printf("Num Updates: %d\n", head->num_updates);
        printf("Max Value: %d\n", head->max_value);
        printf("Min Value: %d\n", head->min_value);
        printf("------------------------\n");
        head = head->next;
    }
    printf("================================\n");
    sem_post(mutex_shm);
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

    exit(0);
}

bool check_msg(char *str)

{
    // Verifica se a string é nula ou vazia
    if (str == NULL || str[0] == '\0')
    {
        return false;
    }

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

bool add_alert(struct alert_list_node **head, char *id, char *key, int min_value, int max_value)
{
    if (head == NULL || id == NULL || key == NULL)
    {
        return false;
    }

    sem_wait(mutex_shm);

    struct alert_list_node *curr = *head;
    while (curr != NULL)
    {
        if (strcmp(curr->key, key) == 0)
        {
            sem_post(mutex_shm);
            // Já existe um alerta associado a esta key
            return false;
        }
        curr = curr->next;
    }

    // Cria um novo nó de alerta e adiciona-o ao início da lista
    struct alert_list_node *new_node = (struct alert_list_node *)malloc(sizeof(struct alert_list_node));
    if (new_node == NULL)
    {
        sem_post(mutex_shm);
        return false;
    }
    strcpy(new_node->id, id);
    strcpy(new_node->key, key);
    new_node->min_value = min_value;
    new_node->max_value = max_value;
    new_node->next = *head;
    *head = new_node;

    sem_post(mutex_shm);

    return true;
}

bool remove_alert(struct alert_list_node **head, char *id)
{
    if (head == NULL || id == NULL || mutex_shm == NULL)
    {
        return false;
    }

    sem_wait(mutex_shm);

    struct alert_list_node *curr = *head;
    struct alert_list_node *prev = NULL;

    while (curr != NULL)
    {
        if (strcmp(curr->id, id) == 0)
        {
            // Encontrou o alerta a remover
            if (prev == NULL)
            {
                // O alerta a remover é o primeiro nó da lista
                *head = curr->next;
            }
            else
            {
                // O alerta a remover não é o primeiro nó da lista
                prev->next = curr->next;
            }
            free(curr);

            sem_post(mutex_shm);
            return true;
        }
        prev = curr;
        curr = curr->next;
    }

    // Não encontrou o alerta a remover
    sem_post(mutex_shm);
    return false;
}

void list_alerts(struct alert_list_node *head)
{
    sem_wait(mutex_shm);

    printf("%-4s %-15s %-4s %-4s\n", "ID", "Key", "MIN", "MAX");
    printf("----------------------------------------\n");
    while (head != NULL)
    {
        printf("%-4s %-15s %-4d %-4d\n", head->id, head->key, head->min_value, head->max_value);
        head = head->next;
    }

    sem_post(mutex_shm);
}

int main()
{
    clear_log();
    Config config = read_config_file("config.txt");
    signal(SIGINT, handle_sigint);

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

    return 0;
}

// Run: gcc -o SystemManager SystemManager.c SystemManagerFuncs.c SystemManager.h log.c log.h && ./SystemManager