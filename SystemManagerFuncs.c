#include "SystemManager.h"

Config read_config_file(char *filename)
{
    Config config;
    FILE *fp;
    int line_number = 1;

    // Print the path we are in
    char cwd[1024];
    getcwd(cwd, sizeof(cwd));
    printf("Current working dir: %s\n", cwd);

    fp = fopen(filename, "r");

    if (fp == NULL)
    {
        printf("Erro ao abrir o arquivo %s.\n", filename);
        exit(EXIT_FAILURE);
    }

    fscanf(fp, "%d\n%d\n%d\n%d\n%d", &config.queue_sz, &config.n_workers, &config.max_keys, &config.max_sensors, &config.max_alerts);

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
