#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "SystemManager.h"

#include <stdio.h>
#include <stdlib.h>

typedef struct
{
    int queue_sz;
    int n_workers;
    int max_keys;
    int max_sensors;
    int max_alerts;
} Config;

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

int main()
{
    Config config = read_config_file("config.txt");
    print_config(config);
    return 0;
}

// Run: gcc -Wall -Wextra -o SystemManager SystemManager.c SystemManagerFuncs.c SystemManager.h && ./SystemManager