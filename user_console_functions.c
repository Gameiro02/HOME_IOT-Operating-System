#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "user_console_functions.h"

void command_stats(int console_identifier)
{
    printf("Console %d: stats\n", console_identifier);
}

void command_reset(int console_identifier)
{
    printf("Console %d: reset\n", console_identifier);
}

void command_sensors(int console_identifier)
{
    printf("Console %d: sensors\n", console_identifier);
}

void command_add_alert(int console_identifier, char *id, char *key, char *min, char *max)
{
    printf("Console %d: add_alert %s %s %s %s\n", console_identifier, id, key, min, max);
}

void command_remove_alert(int console_identifier, char *id)
{
    printf("Console %d: remove_alert %s\n", console_identifier, id);
}

void command_list_alerts(int console_identifier)
{
    printf("Console %d: list_alerts\n", console_identifier);
}

void command_exit(int console_identifier)
{
    printf("Console %d: exit\n", console_identifier);
    exit(0);
}

bool read_command(int console_identifier)
{
    char command[MAX_COMMAND_LENGTH];
    char id[MAX_ID_LENGTH];

    // Read command without the newline character
    fgets(command, MAX_COMMAND_LENGTH, stdin);
    command[strcspn(command, "\n")] = 0;

    // Choose the correct function to execute
    if (strcmp(command, "stats") == 0)
        command_stats(console_identifier);
    else if (strcmp(command, "reset") == 0)
        command_reset(console_identifier);
    else if (strcmp(command, "sensors") == 0)
        command_sensors(console_identifier);
    else if (strcmp(command, "list_alerts") == 0)
        command_list_alerts(console_identifier);
    else if (strcmp(command, "exit") == 0)
        command_exit(console_identifier);
    else if (strncmp(command, "add_alert ", 10) == 0)
    {
        // Check if the id is valid
        char *key, *min, *max;
        key = strtok(command, " ");
        key = strtok(NULL, " ");
        if (strlen(key) < MIN_ID_LENGTH || strlen(key) > MAX_ID_LENGTH)
        {
            printf("ERROR: Invalid id!\n");
        }

        // Get the key
        key = strtok(NULL, " ");
        if (key == NULL)
        {
            printf("ERROR: Invalid key!\n");
        }

        // Get the min
        min = strtok(NULL, " ");
        if (min == NULL)
        {
            printf("ERROR: Invalid min!\n");
        }

        // Get the max
        max = strtok(NULL, " ");
        if (max == NULL)
        {
            printf("ERROR: Invalid max!\n");
        }

        command_add_alert(console_identifier, key, key, min, max);
    }
    else if (strncmp(command, "remove_alert ", 13) == 0)
    {
        char *id;
        id = strtok(command, " ");
        id = strtok(NULL, " ");

        if (strlen(id) < MIN_ID_LENGTH || strlen(id) > MAX_ID_LENGTH)
        {
            printf("ERROR: Invalid id!\n");
        }

        command_remove_alert(console_identifier, id);
    }
    else
        printf("ERROR: Invalid command!\n");
}

void write_to_named_pipe(const char *pipe_name, const char *message)
{
    int fd;

    // Abrir o named pipe em modo de escrita
    fd = open(pipe_name, O_WRONLY);

    // Verificar se houve erro ao abrir o named pipe
    if (fd == -1)
    {
        perror("Erro ao abrir o named pipe");
        exit(EXIT_FAILURE);
    }

    // Escrever a mensagem no named pipe
    write(fd, message, strlen(message));

    // Fechar o descritor de arquivo
    close(fd);
}