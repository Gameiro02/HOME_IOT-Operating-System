#include "SystemManager.h"

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

void worker()
{
    printf("Worker\n");
}

void *sensor_reader_routine(void *arg)
{
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
            printf("Mensagem: %s \n", buffer);
        bzero(buffer, BUFFER_SIZE);
    }
    // Fecha a pipe e finaliza a thread
    close(fd);
    pthread_exit(NULL);
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

    mutex_sensor_pipe = sem_open("mutex_sensor_pipe", O_CREAT, 0777, 1);
    if (mutex_sensor_pipe == SEM_FAILED)
    {
        perror("sem_open: ");
        exit(1);
    }

    inicilize_shared_memory(config);

    print_shared_memory();

    create_named_pipes();

    // Create workers
    for (int i = 0; i < config.n_workers; i++)
    {
        if (fork() == 0)
        {
            worker();
            exit(0);
        }
    }

    // Create the 2 threads: console_reader and sensor_reader
    pthread_t console_reader, sensor_reader;

    pthread_create(&sensor_reader, NULL, sensor_reader_routine, NULL);

    pthread_join(sensor_reader, NULL);

    shmctl(shmid, IPC_RMID, NULL);

    return 0;
}

// Run: gcc -o SystemManager SystemManager.c SystemManagerFuncs.c SystemManager.h && ./SystemManager