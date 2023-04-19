#include "user_console_functions.h"
#include <sys/msg.h>
#include <sys/ipc.h>

int queue_id;

void process_reader()
{
    message buffer;
    while (1)
    {
        // Esperar por uma mensagem da fila de mensagens
        if (msgrcv(queue_id, &buffer, sizeof(buffer), WORKER_TO_CONSOLE, 0) == -1)
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
    int console_identifier;
    key_t key;
    key = ftok(".", QUEUE_KEY);

    // Open the message queue
    queue_id = msgget(key, 0660);
    if (queue_id == -1)
    {
        perror("Erro ao obter o ID da fila de mensagens");
        exit(EXIT_FAILURE);
    }

    printf(" ======================================================MENU=======================================================\n"
           "| COMMAND                                DESCRIPTION                                                              |\n"
           "|-----------------------------------------------------------------------------------------------------------------|\n"
           "| stats                                  Apresenta estatísticas referentes aos dados enviados pelos sensores      |\n"
           "| reset                                  Limpa todas as estatísticas calculadas até ao momento pelo sistema       |\n"
           "| sensors                                Lista todos os sensors que enviaram dados ao sistema                     |\n"
           "| add_alert [id] [chave] [min] [max]     Adiciona uma nova regra de alerta ao sistema                             |\n"
           "| remove_alert [id]                      Remove uma regra de alerta do sistema                                    |\n"
           "| list_alerts                            Lista todas as regras de alerta que existem no sistema                   |\n"
           "| exit                                   Exit the program                                                         |\n"
           " =================================================================================================================\n");

    if (argc != 2)
    {
        printf("ERROR: Console identifier not provided!\n");
        return 1;
    }

    console_identifier = atoi(argv[1]);

    signal(SIGINT, handle_sigint);
    ignore_signals();

    // Create a new process to read the commands from the message queue
    if (fork() == 0)
    {
        process_reader();
        exit(EXIT_SUCCESS);
    }

    while (1)
    {
        read_command(console_identifier);
    }

    return 0;
}

// Run: gcc -o user_console user_console.c user_console_functions.c user_console_functions.h && ./user_console 1