#include "user_console_functions.h"
#include <sys/msg.h>
#include <sys/ipc.h>

int main(int argc, char *argv[])
{
    int console_identifier;
    key_t key;
    message buffer;
    key = ftok(".", QUEUE_KEY);

    // Open the message queue
    int queue_id = msgget(key, 0660);
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

    while (1)
    {

        read_command(console_identifier);

        // Esperar por uma mensagem da fila de mensagens
        // if (msgrcv(queue_id, &buffer, sizeof(buffer.message), console_identifier, 0) == -1)
        // {
        //     perror("Erro ao ler da fila de mensagens");
        //     exit(EXIT_FAILURE);
        // }

        printf("Mensagem recebida: %s\n", buffer.message);
    }

    return 0;
}

// Run: gcc -o user_console user_console.c user_console_functions.c user_console_functions.h && ./user_console 1