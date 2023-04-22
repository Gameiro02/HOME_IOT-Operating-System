# User Console
Perguntar ao stor se para criar o processo "user_console" basta usa-lo como um programa independente do main

# Sensor
Pergutar se no SENSOR a chave tem de ter obrigatoriamente um digito, uma letra e o "_"
Perguntar ao stor se o valor do sensor tem de ser um valor aleatorio entre o minimo e o maximo ou se pode ser (s->min + s->max) / 2
Perguntar ao stor se pode na parte do signal handler se pode usar uma variavel global.
Perguntar ao stor como é que vou passar os semaforos para todos os ficehiiros




# Respostas do Stor
É preciso validar que a chave do sensor apenas tem caracteres alfanumericos e _
Nas nammed pipes não são necessários semáforos
O user_console/sensor é como era de esperar um programa compilado
Inicialmete apenas são necessários 3 semáforos:

- O semáforo da shared_memory:
- O semáforo do log
- O semaforo da Queue

Mais para a frente, para operações mais complexas vão ser necessários mais semáforos

No alerts watcher é preciso meter uma vairavel de condição para nao haver busy waiting


---------------------------------------------


Perguntar ao stor como e que se vai sinconizar a fila com vairos workers DONE
chamar a funcao de terminar qunado da erro DONE 
fork e pthread create verificar se falhou DONE
abrir so uma vez o ficheiro de log DONE
Erros do fork DONE
alterar o tamanho da shm DONE
Perguntar ao stor se e suposto aparecer 5 vezes exiting lalalala quando se faz control c DONE
Usar o type da estrutura de mensagem para identificar o user_console que queremos DONE
adicionar a cena de remover threads ao termiate DONE


Perguntar ao stor porque e que quando se printa o valor do sensor reader vem aquilo com caracteres a mais as vezes (???)