#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "user_console_functions.h"
#include "SystemManager.h"
#include <signal.h>

// Function to the process senssor

#define SENSOR_PIPE "sensor_pipe"

struct sensor
{
    char id[MAX_ID_LENGTH];
    int interval;
    char key[MAX_ID_LENGTH];
    int min;
    int max;
    int num_messages;
};

struct sensor s1;

void handle_sigint()
{
    printf("Received SIGINT signal. Exiting...\n");
    exit(0);
}

void handle_sigtstp()
{
    printf("Number of messages sent: %d\n", s1.num_messages);
}

char *sensor_to_string(struct sensor *s)
{
    char *str = malloc(sizeof(char) * 100); // Alocar espaço suficiente para a string resultante

    int random_value = rand() % (s->max - s->min + 1) + s->min; // Gerar um valor aleatório entre min e max

    sprintf(str, "%s#%s#%d", s->id, s->key, random_value); // Formatar a string com as informações do sensor
    return str;
}

void ignore_signals()
{
    struct sigaction sa;
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    int i;
    for (i = 1; i <= 64; i++)
    {
        if (i == SIGINT || i == SIGTSTP)
        {
            continue;
        }
        if (sigaction(i, &sa, NULL) == -1)
        {
            // Ignorar sinais que não são suportados
            if (errno == EINVAL)
            {
                continue;
            }
            perror("Erro ao definir acao do sinal");
            exit(EXIT_FAILURE);
        }
    }

    printf("Todos os sinais, exceto SIGINT e SIGTSTP, serao ignorados.\n");

    return;
}

int main(int argc, char *argv[])
{
    s1.num_messages = 0;

    if (argc != 6)
    {
        printf("Wrong number of arguments\n");
        return 1;
    }

    if (strlen(argv[1]) < MIN_ID_LENGTH || strlen(argv[1]) > MAX_ID_LENGTH)
    {
        printf("Id must have between %d and %d characters\n", MIN_ID_LENGTH, MAX_ID_LENGTH);
        return 1;
    }

    strcpy(s1.id, argv[1]);

    s1.interval = atoi(argv[2]);

    if (s1.interval < 0)
    {
        printf("Interval must be greater than or equal to 0\n");
        return 1;
    }

    if (strlen(argv[3]) < MIN_ID_LENGTH || strlen(argv[3]) > MAX_ID_LENGTH)
    {
        printf("Key must have between %d and %d characters\n", MIN_ID_LENGTH, MAX_ID_LENGTH);
        return 1;
    }

    strcpy(s1.key, argv[3]);

    s1.min = atoi(argv[4]);
    s1.max = atoi(argv[5]);

    if (s1.min > s1.max)
    {
        printf("Min must be less than or equal to max\n");
        return 1;
    }

    // Signal handlers
    signal(SIGINT, handle_sigint);
    signal(SIGTSTP, handle_sigtstp);
    ignore_signals();

    // Create the String to Send
    char *str;

    // Send to the pipe
    int pipe = open(SENSOR_PIPE, O_WRONLY);

    if (pipe == -1)
    {
        printf("Error opening pipe\n");
        return 1;
    }

    while (1)
    {
        str = sensor_to_string(&s1);
        sleep(s1.interval);
        write(pipe, str, strlen(str));
        printf("Sent: %s\n", str);
        s1.num_messages++;
    }

    close(pipe);
}

// run: gcc sensor.c -o sensor  && ./sensor SENS1 3 HOUSETEMP 10 100