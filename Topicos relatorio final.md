# Meta 2

# System Manager
O System Manager é um processo que cria todos os mecanismos de sincronização e comunicação entre os processos, bem como todos os processos worker, alerts watcher e as 3 threads(Dispatcher, Sensor reader e console reader)

# Sensor Reader
O sensor Reader le os comandos dos sensores e coloca-os na internal queue.

# Console Reader
O console reader le os comandos do utilizador e coloca-os na internal queue.
Os comandos que o utilizador pode executar têm mais prioridade que os comandos dos sensores.
Para implementar isto usa-se uma variavel de prioridade em cada no da internal queue.

# Dispatcher
O dispatcher é uma thread que lê os comandos da internal queue e envia-os para o worker através dos unnamed pipes especificos para cada worker. O worker é escolhido de forma aleatoria do array de workers livres. Este é mantido e atualizado na shared memory.

# Worker
O worker é um processo que recebe os comandos do dispatcher e executa-os. No caso de ser um comando de um sensor, o worker envia os dados para a key list que esta na shared memory. No caso de ser um comando do utilizador, o worker executa o comando e envia a resposta para a mensagem queue, o type da mensagem é o id do user que enviou o comando.

# Alerts Watcher
O alerts watcher é um processo que verifica se existem alertas na alert list e verifica a key list para ver se os valores dos sensores estao dentro do alcance do alerta. Se não estiverem dentro do alcance, o alerts watcher envia uma mensagem para a user console atraves da message queue a dizer que o alerta foi acionado. O alerta é apenas enviado para o user id guardado na alert list.

# User Console
A user console abre a message queue com a key 1234 e cria um processo que vai ficar a ler mensagens da message queue e a escreve-las no ecra. Depois disto mostra o menu ao utilizador e envia as mensagens pelo nammed pipe para o system manager.

# Sensor
O sensor envia periodicamente os dados para o sensor reader atraves do named pipe.
Quado é enviado o sinal sigstop, o sensor mostra no ecra o numero de mensagens enviadas.

# Ficheiro de Log
O ficheiro de log tem comunicação com o System Manager servindo para escrever todas as informações que o System Manager recebe e serem analisadas porteriormente. Para que as informações não sejam corrompidas, é necessário sincronizar o acesso ao ficheiro de log, para isso, é utilizado um semáforo. O semáforo que usamos tem o nome de `log_sem` e é criado no System Manager.


# Internal queue
Na internal queue colocamos um mutex, da forma pthread_mutex_t, para sincronizar o acesso à fila. O nome da varivel é `internal_queue_mutex`. A este mutex, vamos fazer lock quando fazemos alguma operação na fila, e unlock quando terminamos a operação. Por exemplo, quando adicionamos um elemento à fila, fazemos lock, adicionamos o elemento, e depois fazemos unlock. Quando retiramos um elemento da fila, fazemos lock, retiramos o elemento, e depois fazemos unlock.Tambem temos uma variavel de condicao 'internal_queue_cond' para indicar que a internal queue tem elementos para serem lidos.

# Memória Partilhada
A memória partilhada tem comunicação com o System Manager, Worker e Alert Watcher sendo utilizada para guardar os dados dos sensores e enviar os dados para o servidor. Para que os dados não sejam corrompidos, é necessário sincronizar o acesso à memória partilhada, para isso, é utilizado um semáforo. O semáforo que usamos tem o nome de `mutex_shm` e é criado no System Manager. A memória partilhada contem o ficheiro de configuração, a key list, a alert list e o array de workers livres.

# Sinais
Todos os processos ignoram todos os sinais, com excessao do sigint e do sigstop.
No caso de receberem o sigint, todos os processos limpam os seus recursos e terminam.
No caso de receberem o sigstop, os processos são suspensos, com a excepção do sensor que mostra no ecra o numero de mensagens enviadas.

# Mecanismos de sicronização
Os mecanismos de sincronização utilizados foram:

mutex_shm - semaforo para sincronizar o acesso à shared memory
log_sem - semaforo para sincronizar o acesso ao ficheiro de log

check_alert_sem - semaforo para indicar que se pode verificar os alertas, ou seja, que a key list está atualizada e a alert list não está vazia

internal_queue_mutex - mutex para sincronizar o acesso à internal queue

internal_queue_cond - variavel de condicao para indicar que a internal queue tem elementos para serem lidos

internal_queue_mutex - mutex para sincronizar o acesso à internal queue


# Tempo total dispendido
Para a realização destre projeto, foram necessárias, em estimativa, entre 40 a 50 horas de trabalho. Fazendo contas, gastamos cerca de 20 a 25 horas cada um. 