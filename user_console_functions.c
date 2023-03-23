#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "user_console_functions.h"
#include "log.h"

bool read_command(int console_identifier)
{
    char command[MAX_COMMAND_LENGTH];
    char id[MAX_ID_LENGTH];

    // Read command without the newline character
    fgets(command, MAX_COMMAND_LENGTH, stdin);
    command[strcspn(command, "\n")] = 0;

    // Choose the correct function to execute
    if (strcmp(command, "stats") == 0)
    {
        write_to_named_pipe(CONSOLE_PIPE, command);
    }
    else if (strcmp(command, "reset") == 0)
    {
        write_to_named_pipe(CONSOLE_PIPE, command);
    }
    else if (strcmp(command, "sensors") == 0)
    {
        write_to_named_pipe(CONSOLE_PIPE, command);
    }
    else if (strcmp(command, "list_alerts") == 0)
    {
        write_to_named_pipe(CONSOLE_PIPE, command);
    }
    else if (strcmp(command, "exit") == 0)
    {
        exit(0);
    }
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

        write_to_named_pipe(CONSOLE_PIPE, command);
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

        write_to_named_pipe(CONSOLE_PIPE, command);
    }
    else
    {
        char *msg = malloc(strlen(command) + 50);
        sprintf(msg, "INVALID COMMAND => %s", command);
        log_message(msg);
        free(msg);
    }
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