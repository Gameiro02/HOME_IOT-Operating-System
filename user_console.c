#include "user_console_functions.h"
#include <sys/msg.h>
#include <sys/ipc.h>

int queue_id;
int console_identifier;

void process_reader()
{
    message buffer;
    while (1)
    {
        // Esperar por uma mensagem da fila de mensagens
        if (msgrcv(queue_id, &buffer, sizeof(buffer), console_identifier, 0) == -1)
        {
            perror("Erro ao ler da fila de mensagens");
            exit(EXIT_FAILURE);
        }

        printf("%s\n", buffer.message);

        // if we receive a message saying "terminate", we terminate the process
        if (strcmp(buffer.message, "terminate") == 0)
        {
            printf("Terminating console...\n");
            exit(EXIT_SUCCESS);
        }
    }
}

// Signal handler for SIGINT
void handle_sigint()
{
    printf("Received SIGINT signal. Exiting...\n");
    exit(0);
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
    if (argc != 2)
    {
        printf("ERROR: Console identifier not provided!\n");
        return 1;
    }

    console_identifier = atoi(argv[1]);

    // check if the console identifier is an integer value > 0
    if (console_identifier <= 0)
    {
        printf("ERROR: Console identifier must be an integer value > 0!\n");
        return 1;
    }

    key_t key;
    key = ftok(".", QUEUE_KEY);

    // Open the message queue
    queue_id = msgget(key, 0660);
    if (queue_id == -1)
    {
        perror("Erro ao obter o ID da fila de mensagens");
        exit(EXIT_FAILURE);
    }

    printf("=========== MENU ===========\n"
           "| COMMAND             | DESCRIPTION                                       |\n"
           "|-------------------- |---------------------------------------------------|\n"
           "| stats               | Apresenta estatísticas dos sensores               |\n"
           "| reset               | Limpa todas as estatísticas do sistema            |\n"
           "| sensors             | Lista todos os sensores                           |\n"
           "| add_alert [id] [ch] | Adiciona uma nova regra de alerta                 |\n"
           "| remove_alert [id]   | Remove uma regra de alerta                        |\n"
           "| list_alerts         | Lista todas as regras de alerta                   |\n"
           "| exit                | Sai do programa                                    |\n"
           "============================\n");

    // Create a new process to read the commands from the message queue
    if (fork() == 0)
    {
        process_reader();
        exit(EXIT_SUCCESS);
    }
    signal(SIGINT, handle_sigint);
    ignore_signals();

    while (1)
    {
        read_command(console_identifier);
    }

    return 0;
}

// Run: gcc -o user_console user_console.c user_console_functions.c user_console_functions.h && ./user_console 1