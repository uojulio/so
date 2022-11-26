#include "balcao.h"

#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <time.h>


int MAXCLIENTES;
int  MAXMEDICOS;


int       utentes_fd;
int especialistas_fd; 


int balcao_para_classificador[2];
int classificador_para_balcao[2];

//  MONITORIZAR A FREQUÊNCIA
pthread_t     freq_thread;
pthread_mutex_t freq_lock;
int frequencia = 20;


//  MONITORIZAR O HEARTBEAT
pthread_t     heartbeat_thread;
pthread_mutex_t heartbeat_lock;


bool iniciar_balcao() {
static int init = 0;
if(init) return true;

    if(pthread_mutex_init(&freq_lock, NULL) != 0 ||
        pthread_mutex_init(&heartbeat_lock, NULL) != 0) {
        PERROR("Erro interno. {MUTEX}");
        return false;
    }

    if(pthread_create(&freq_thread, NULL, &freq, NULL) != 0 ||
        pthread_create(&heartbeat_thread, NULL, &heartbeat, NULL) != 0) {
        PERROR("Erro interno. {THREAD}");
    }

    setenv(CLIENTES_ENV, DEFAULT_MAXCLIENTES, 0);
    setenv( MEDICOS_ENV,  DEFAULT_MAXMEDICOS, 0);

    MAXCLIENTES = atoi(getenv(CLIENTES_ENV));
    MAXMEDICOS  = atoi(getenv( MEDICOS_ENV));

    if(access(      PIPE_UTENTES, F_OK) == 0 ||
       access(PIPE_ESPECIALISTAS, F_OK) == 0) {
        PERROR("Multiplas instancias do balcao.");
        return false;
    }

    mkfifo(      PIPE_UTENTES, 0600);
    mkfifo(PIPE_ESPECIALISTAS, 0600);

          utentes_fd = open(      PIPE_UTENTES, O_RDWR);
    especialistas_fd = open(PIPE_ESPECIALISTAS, O_RDWR);

    signal(SIGUSR1, signal_classificador);

    pipe(balcao_para_classificador);
    pipe(classificador_para_balcao);

    if(fork() == 0) {
        iniciar_classificador(0);
    }

    init = 1<<0;
    return true;
}

void libertar_utentes() {
    struct utente * aux = lista_utentes;
    while(aux)
    {
        struct mensagem resposta;
        montar_pacote(&resposta, 
        BALCAO_NOME,
        aux->nome,
        BALCAO_ID, 
        aux->pid,
        SAIR_REPLY,
        strlen(SAIR_REPLY),
        MENSAGEM_SAIR);
        enviar_pacote(&resposta);

        lista_utentes = lista_utentes->next;
        free(aux);
        aux = lista_utentes;
    }
    lista_utentes = 0;
}

void libertar_especialistas() {
    struct especialista * aux = lista_especialistas;
    while(aux)
    {
        struct mensagem resposta;
        montar_pacote(&resposta, 
        BALCAO_NOME,
        aux->nome,
        BALCAO_ID, 
        aux->pid,
        SAIR_REPLY,
        strlen(SAIR_REPLY),
        MENSAGEM_SAIR);
        enviar_pacote(&resposta);

        lista_especialistas = lista_especialistas->next;
        free(aux);
        aux = lista_especialistas;
    }
    lista_especialistas = 0;
}

void encerrar_balcao() {
    libertar_consultas();

    libertar_utentes();
    libertar_especialistas();

    encerrar_classificador();

    close(utentes_fd);
    close(especialistas_fd);

    unlink(PIPE_UTENTES);
    unlink(PIPE_ESPECIALISTAS);

    pthread_cancel(freq_thread);
    pthread_mutex_destroy(&freq_lock);

    pthread_cancel(heartbeat_thread);
    pthread_mutex_destroy(&heartbeat_lock);

    printf(INFO"Sistema encerrado.\n");
}

void parse_str(char * str) {
    char straux[DATA_LEN];
    memset(straux, 0, sizeof straux);

    for(int i = 0, k = 0; str[i] != '\0'; ++i)
    {
        if(str[i] != ' ')
        {
            if(str[i] == '\n') continue;

            straux[k++] = str[i];
        }
        if(str[i] == ' ')
        {
            if(str[i+1] == ' ') continue;
            straux[k++] = str[i];
        }
    }
    strcpy(str, straux);
}

int contar_palavras(char * str) {
    int contador = 0;
    for(int i = 0; str[i] != '\0'; ++i)
    {
        if(str[i] == ' ') 
        {
            ++contador;
        }
    }
    return ++contador;
}


void mostrar_utentes() {
    PINFO("Lista de utentes:");

    if(lista_utentes == 0) {
        printf("Nenhum utente registado.\n");
    }
    else {
        struct utente * aux;
        for(aux = lista_utentes; aux != 0; aux = aux->next) {
            printf("\tPID{"MAG"%d"CRT"} NOME{"MAG"%s"CRT"} PRIORIDADE{",aux->pid, aux->nome);
            switch(aux->prioridade) {
                case 1: printf(RED);
                break;
                case 2: printf(YLW);
                break;
                case 3: printf(GRN);
                break;
                default:
                break;
            }
            printf("%d"CRT"} ESPECIALIDADE{"MAG"%s"CRT"} ESTADO{", aux->prioridade, aux->especialidade);
            if(aux->estado) printf(BLE"EM ATENDIMENTO"CRT"}\n");
            else printf(YLW"EM ESPERA"CRT"}\n");
        }

        printf(INFO"Encontram se "BLE"%d"CRT" utentes no sistema. (max "MAG"%d"CRT")\n", contagem_utentes(), MAXCLIENTES);
    }
}

void mostrar_especialistas() {
    PINFO("Lista de especialistas:");
    if(lista_especialistas == 0) {
        printf("Nenhum especialista registado.\n");
    }
    else {
        struct especialista * aux;
        for(aux = lista_especialistas; aux != 0; aux = aux->next) {
            printf("\tPID{"MAG"%d"CRT"} NOME{"MAG"%s"CRT"} ESPECIALIDADE{"MAG"%s"CRT"} ESTADO{", aux->pid, aux->nome, aux->especialidade);
            if(aux->estado) printf(BLE"EM ATENDIMENTO"CRT"}\n");
            else printf(YLW"EM ESPERA"CRT"}\n");
        }

        printf(INFO"Encontram se "BLE"%d"CRT" especialistas no sistema. (max "MAG"%d"CRT")\n", contagem_especialistas(), MAXMEDICOS);
    }
}

void inserir_utente(struct utente * u) {
    if(!lista_utentes) {
        lista_utentes = u;
    }
    else {
        struct utente * aux;
        for(aux = lista_utentes; aux != 0; aux = aux->next) {
            if(aux->prioridade > u->prioridade) {
                u->prev = aux->prev;
                u->next = aux;

                if(aux->prev) {
                    aux->prev->next = u;
                }
                else {
                    lista_utentes = u;
                }

                aux->prev = u;
                break;
            }
            else {
                if(aux->next) {
                    continue;
                }

                aux->next = u;
                u->prev = aux;
                u->next = 0;
                break;
            }
        }
    }
}

void inserir_especialista(struct especialista * e) {
    if(!lista_especialistas) {
        lista_especialistas = e;
    }
    else {
        struct especialista * aux;
        for(aux = lista_especialistas; aux->next != 0; aux = aux->next) {}
        aux->next = e;
        e->prev = aux;
    }
}


struct utente * procurar_utente(int pid) {
    struct utente * aux;
    for(aux = lista_utentes; aux != 0; aux = aux->next) {
        if(aux->pid == pid) return aux;
    }
    return 0;
}

struct especialista * procurar_especialista(int pid) {
    struct especialista * aux;
    for(aux = lista_especialistas; aux != 0; aux = aux->next) {
        if(aux->pid == pid) return aux;
    }
    return 0;
}

struct utente * gerar_utente(int pid, const char * nome, int prioridade, const char * especialidade) {
    struct utente * u = malloc(sizeof(struct utente));

    u->pid              = pid;
    memcpy(u->nome, nome, NOME_LEN);
    u->prioridade       = prioridade;
    u->especialidade    = especialidade;

    u->prev             = 0;
    u->next             = 0;
    u->estado           = false;

    return u;
}

struct especialista * gerar_especialista(int pid, const char * nome, const char * especialidade) {
    struct especialista * e = malloc(sizeof(struct especialista));

    e->pid = pid;
    memcpy(e->nome, nome, NOME_LEN);
    e->especialidade = especialidade;

    e->prev = 0;
    e->next = 0;
    e->estado = false;

    time(&e->TTL);

    return e;
}

void signal_classificador(int signal) {
    PERROR("Programa classificador em falta.");
    encerrar_balcao();
    exit(1);
}

void iniciar_classificador(int debug) {

    dup2(classificador_para_balcao[1], STDOUT_FILENO);
    dup2(balcao_para_classificador[0], STDIN_FILENO);

    close(classificador_para_balcao[0]);
    close(balcao_para_classificador[1]);

    if(debug) execl("classificador", "classificador", "debug", NULL);
    else execl("classificador", "classificador", NULL);

    kill(getppid(), SIGUSR1);

    exit(1);
}

void encerrar_classificador() {
    const char * fim = "#fim";
    write(balcao_para_classificador[1], fim, strlen(fim));
}

void obtem_classificacao(struct utente * u, const char * sintomas) {
    char classificacao[DATA_LEN];

    write(balcao_para_classificador[1], sintomas, strlen(sintomas));
    read(classificador_para_balcao[0], classificacao, DATA_LEN);

    char delim[] = " \n";
    
    u->especialidade = 0;
    
    char * aux = strtok(classificacao, delim);
    for(int i = 0; i < 5; ++i) {
        if(strcmp(ESPECIALIDADES[i], aux) == 0) {
            u->especialidade = ESPECIALIDADES[i];
        }
    }
    if(!u->especialidade) u->especialidade = ESPECIALIDADES[4];
    
    u->prioridade = atoi(strtok(NULL, delim));
}

int contagem_utentes() {
    int contador = 0;

    struct utente * aux;
    for(aux = lista_utentes; aux != 0; aux = aux->next) {
        contador++;
    }

    return contador;
}

int contagem_especialistas() {
    int contador = 0;

    struct especialista * aux;
    for(aux = lista_especialistas; aux != 0; aux = aux->next) {
        contador++;
    }

    return contador;
}

void comandos() {
    PINFO("Lista de comandos permitidos");

    for(int i = 0; i < (sizeof comandoss / sizeof(struct comando)); ++i) {
        printf(BLE"%-20s"CRT" - %s\n", comandoss[i].nome, comandoss[i].descricao);
    }
}

void verificar_comando(char * input) {
    if(strlen(input) == 0) return;

    char straux[DATA_LEN];
    strcpy(straux, input);

    char delim[] = " \n";
    char * ptr = strtok(straux, delim);


    int cmd_len = (sizeof comandoss / sizeof(struct comando));
    for(int i = 0; i < cmd_len; ++i) {
        if(strcmp(comandoss[i].nome, ptr) == 0) {
            comandoss[i].func(input);
            return;
        }
    }

    printf(ERROR"Nenhum comando com nome: "RED"%s"CRT"\n", input);
}

void mostrar_utentes_cmd (char * x) {
    mostrar_utentes();
}

void mostrar_especialistas_cmd  (char * x) {
    mostrar_especialistas();
}

void remover_utente_cmd (char * input) {
    if(contar_palavras(input) != 2) {
        PINFO("Utilize o seguinte formato para o comando:");
        PRINT(stdout, FORMAT("delut {PID}"));
        return;
    }

    char delim[] = " \n";
    char * ptr = strtok(input, delim);
    
    int pid = atoi(strtok(NULL, delim));

    internal_remover_utente(pid);
}

void remover_especialista_cmd (char * input) {
    if(contar_palavras(input) != 2) {
        PINFO("Utilize o seguinte formato para o comando:");
        PRINT(stdout, FORMAT("delesp {PID}"));
        return;
    }

    char delim[] = " \n";
    char * ptr = strtok(input, delim);
    
    int pid = atoi(strtok(NULL, delim));

    internal_remover_especialista(pid);
}

void remover_par_cmd (char * input) {
    if(contar_palavras(input) != 2)
    {
        PINFO("Utilize o seguinte formato para o comando:");
        printf("delcon {"MAG"ID"CRT"}\n");
        return;
    }

    char delim[] = " \n";
    char * ptr = strtok(input, delim);
    
    int id = atoi(strtok(NULL, delim));
    internal_remover_par(id);
    
    emparelhar();
}

void comandos_cmd (char * x) {
    comandos();
}

void limpar_terminal_cmd (char * x) {
    printf(CLR);
}

void encerrar_balcao_cmd (char * x) {
    encerrar_balcao();
}

void mostrar_pares_cmd (char * x) {
    mostrar_pares();
}

void utentes_mensagem(struct mensagem * pacote) {
    PINFO("Mensagem recebida:");
    mostrar_pacote(pacote);

    struct mensagem resposta = {0};
    char resultado[DATA_LEN];

    if(pacote->informacao.reservado == MENSAGEM_REGISTO) {
        if(adicionar_utente(pacote, resultado) == true) {
            montar_pacote(&resposta, 
            BALCAO_NOME,
            pacote->informacao.origem.nome,
            BALCAO_ID, 
            pacote->informacao.origem.pid, 
            resultado, 
            strlen(resultado), 
            MENSAGEM_SUCESSO);
        }
        else {
            montar_pacote(&resposta, 
            BALCAO_NOME,
            pacote->informacao.origem.nome,
            BALCAO_ID, 
            pacote->informacao.origem.pid, 
            resultado, 
            strlen(resultado), 
            MENSAGEM_FALHA);
        }
        enviar_pacote(&resposta);
        emparelhar();
    }
    else {
        struct utente * u = procurar_utente(pacote->informacao.origem.pid);
        if(u) 
        {
            switch(pacote->informacao.reservado) {
                struct par * p;

                case MENSAGEM_SAIR_CONSULTA:
                p = procurar_par(u->pid);
                if(p) {
                    internal_remover_par(p->id);
                }
                emparelhar();
                break;
                case MENSAGEM_SAIR:
                internal_remover_utente(pacote->informacao.origem.pid);
                break;
                default:
                PINFO("Mensagem invalida.");
                break;
            }
        }
        else {
            PINFO("Mensagem descartada.");
        }
    }

    return;
}

void especialistas_mensagem(struct mensagem * pacote) {
    if(pacote->informacao.reservado != MENSAGEM_HEARTBEAT) {
        PINFO("Mensagem recebida:");
        mostrar_pacote(pacote);
    }

    struct mensagem resposta = {0};

    char resultado[DATA_LEN];

    if(pacote->informacao.reservado == MENSAGEM_REGISTO) {
        if(adicionar_especialista(pacote, resultado) == true) {
            montar_pacote(&resposta, 
            BALCAO_NOME,
            pacote->informacao.origem.nome,
            BALCAO_ID, 
            pacote->informacao.origem.pid, 
            resultado, 
            strlen(resultado), 
            MENSAGEM_SUCESSO);
        }
        else {
            montar_pacote(&resposta, 
            BALCAO_NOME,
            pacote->informacao.origem.nome,
            BALCAO_ID, 
            pacote->informacao.origem.pid, 
            resultado, 
            strlen(resultado), 
            MENSAGEM_FALHA);
        }
        enviar_pacote(&resposta);
        emparelhar();
    }
    else {
        struct especialista * e = procurar_especialista(pacote->informacao.origem.pid);
        if(e) 
        {
            time(&e->TTL); // atualizamos o heartbeat
            switch(pacote->informacao.reservado) {
                struct par * p;

                case MENSAGEM_SAIR_CONSULTA:
                p = procurar_par(e->pid);
                if(p) {
                    internal_remover_par(p->id);
                }
                emparelhar();
                break;
                case MENSAGEM_SAIR:
                internal_remover_especialista(pacote->informacao.origem.pid);
                emparelhar();
                break;
                case MENSAGEM_HEARTBEAT:
                printf("RECEBI MENSAGEM DE HEARTBEAT de "MAG"%s"CRT" ["MAG"%d"CRT"]\n", 
                    pacote->informacao.origem.nome, 
                    pacote->informacao.origem.pid);
                printf("NOVO HEARTBEAT: %0ld\n", e->TTL);
                break;
                default:
                PINFO("Mensagem invalida.");
                break;
            }
        }
        else {
            PINFO("Mensagem descartada.");
        }
    }
    return;
}


bool adicionar_utente(struct mensagem * pacote, char * resultado) {
    if(contagem_utentes() >= MAXCLIENTES) {
        strcpy(resultado, REGISTO_LOTACAO_REPLY);
        PINFO("Lotacao de utentes atingida.");
        PERROR("Registo abortado.");
        return false;
    }

    struct utente * u = criar_utente(pacote);  

    if(validar_utente(u, resultado)) {
        inserir_utente(u);
        PSUCCESS("Utente registado com sucesso!");
        return true;
    }
    else {
        PERROR("Registo abortado.");
        free(u);
        return false;
    }
}

bool adicionar_especialista(struct mensagem * pacote, char * resultado) {
    if(contagem_especialistas() >= MAXMEDICOS) {
        strcpy(resultado, REGISTO_LOTACAO_REPLY);
        PINFO("Lotacao de especialistas atingida.");
        PERROR("Registo abortado.");
        return false;
    }
    struct especialista * e = criar_especialista(pacote);

    if(validar_especialista(e, resultado)) {
        inserir_especialista(e);
        PSUCCESS("Especialista registado com sucesso!");
        return true;
    }
    else {
        strcpy(resultado, REGISTO_INVALIDO_PID_REPLY);
        PERROR("Registo abortado.");
        free(e);
        return false;
    }
}

struct utente * criar_utente(struct mensagem * pacote) {
    struct utente u;

    parse_str(pacote->informacao.origem.nome);
    memcpy(u.nome, pacote->informacao.origem.nome, NOME_LEN);

    u.pid = pacote->informacao.origem.pid;

    obtem_classificacao(&u, pacote->conteudo);

    return gerar_utente(u.pid, u.nome, u.prioridade, u.especialidade);
}

struct especialista * criar_especialista(struct mensagem * pacote) {
    struct especialista e;

    parse_str(pacote->informacao.origem.nome);
    memcpy(e.nome, pacote->informacao.origem.nome, NOME_LEN);
    
    e.pid = pacote->informacao.origem.pid;

    parse_str(pacote->conteudo);
    e.especialidade = 0;
    for(int i = 0; i < 5; ++i) {
        if(strcmp(ESPECIALIDADES[i], pacote->conteudo) == 0) {
            e.especialidade = ESPECIALIDADES[i];
        }
    }
    if(!e.especialidade) e.especialidade = ESPECIALIDADES[4];

    return gerar_especialista(e.pid, e.nome, e.especialidade);
}

bool validar_utente(struct utente * u, char * resultado) {
    if(procurar_utente(u->pid)) {
        strcpy(resultado, REGISTO_INVALIDO_PID_REPLY);
        printf(INFO"Existe um utente registado com o mesmo PID{%d}.\n", u->pid);
        return false;
    }

    if(contagem_utentes_especialidade(u->especialidade) >= LIMITE_ESPERA) {
        strcpy(resultado, REGISTO_INVALIDO_LIMITE_REPLY);
        return false;
    }
    strcpy(resultado, REGISTO_SUCESSO_REPLY);
    return true;
}

bool validar_especialista(struct especialista * e, char * resultado) {

    if(procurar_especialista(e->pid)) {
        strcpy(resultado, REGISTO_INVALIDO_PID_REPLY);
        printf(INFO"Existe um especialista registado com o mesmo PID{%d}.\n", e->pid);
        return false;
    }

    strcpy(resultado, REGISTO_SUCESSO_REPLY);
    return true;
}

void internal_remover_utente(int pid) {

    struct utente * aux = procurar_utente(pid);

    if(aux) {
        if(aux->estado) {
            printf(INFO"O utente com PID{%d} encontra-se numa consulta.\n", aux->pid);
            PERROR("O utente nao foi removido.");
        }
        else {
            struct mensagem resposta;
            montar_pacote(&resposta, 
            BALCAO_NOME, 
            aux->nome,
            BALCAO_ID,
            aux->pid, 
            SAIR_REPLY, 
            strlen(SAIR_REPLY), 
            MENSAGEM_SAIR);
            enviar_pacote(&resposta);  // ignoro o return porque vou remover o utente de qualquer maneira

            if(aux->prev) {
                aux->prev->next = aux->next;
            }
            if(aux->next) {
                aux->next->prev = aux->prev;
            }
            if(aux->prev == 0) {
                lista_utentes = aux->next;
            }
            printf(SUCCESS"Utente com PID{%d} removido da lista de utentes.\n", aux->pid);
            free(aux);
        }
    }
    else {
        printf(INFO"Nenhum utente registado com PID{%d}\n", pid);
    }
}

void internal_remover_especialista(int pid) {

    struct especialista * aux = procurar_especialista(pid);

    if(aux) {
        if(aux->estado) {
            printf(INFO"O especialista com PID{%d} encontra-se numa consulta.\n", aux->pid);
            PERROR("O especialistas nao foi removido.");
        }
        else{
            struct mensagem resposta;
            montar_pacote(&resposta,
            BALCAO_NOME, 
            aux->nome,
            BALCAO_ID, 
            aux->pid, 
            SAIR_REPLY, 
            strlen(SAIR_REPLY), 
            MENSAGEM_SAIR);
            enviar_pacote(&resposta);

            if(aux->prev) {
                aux->prev->next = aux->next;
            }
            if(aux->next) {
                aux->next->prev = aux->prev;
            }
            if(aux->prev == 0) {
                lista_especialistas = aux->next;
            }
            printf(SUCCESS"Especialista com PID{%d} removido da lista de especialistas.\n", aux->pid);
            free(aux);
        }
    }
    else {
        printf(INFO"Nenhum especialista registado com PID{%d}\n", pid);
    }
}

int contagem_utentes_especialidade(const char * especialidade) {
    struct utente * aux;
    int contador = 0;
    for(aux = lista_utentes; aux != 0; aux = aux->next)
    {
        if(aux->especialidade == especialidade) ++contador;
    }
    return contador;
}



// POR IMPLEMENTAR
void emparelhar() {
    
    if(lista_utentes && lista_especialistas) {
        struct especialista * e;
        struct       utente * u;
        for(e = lista_especialistas; e != NULL; e = e->next) {
            if(e->estado == false) { // se o especialista não estiver em consulta...
                for(u = lista_utentes; u != NULL; u = u->next) {
                    if(u->estado== false) { // se o utente não estiver em consulta...
                        if(u->especialidade == e->especialidade) { // se as especialidade forem iguais...
                            goto MATCH;
                        }
                    }
                }
            }
        }
        goto QUIT;
MATCH:
        adicionar_par(u, e);

        struct mensagem resposta;
        char resultado[DATA_LEN];

        memset(resultado, 0, DATA_LEN);
        sprintf(resultado, "%d", e->pid);

        montar_pacote(&resposta, 
        e->nome,
        u->nome,
        BALCAO_ID, 
        u->pid, 
        resultado, 
        strlen(resultado), 
        MENSAGEM_EMPARELHAR);
        enviar_pacote(&resposta);

        memset(resultado, 0, DATA_LEN);
        sprintf(resultado, "%d", u->pid);

        montar_pacote(&resposta, 
        u->nome,
        e->nome,
        BALCAO_ID, 
        e->pid, 
        resultado, 
        strlen(resultado), 
        MENSAGEM_EMPARELHAR);
        enviar_pacote(&resposta);

        PINFO("Emparelhamento concluido.");
    }
QUIT:
    return;
}


void adicionar_par(struct utente * u, struct especialista * e) {
static int contador = 0;
    if(u->estado || e->estado) return;

    struct par * p = gerar_par(u, e, contador++);

    p->especialista->estado = true;
    p->utente->estado = true;

    inserir_par(p);
}


struct par * encontrar_par(int id) {
    struct par * p_aux;
    for(p_aux = lista_pares; p_aux != NULL; p_aux = p_aux->next) {
        if(p_aux->id == id) {
            return p_aux;
        }
    }
    return NULL;
}

void info_utente(int pid) {
    struct utente * aux = procurar_utente(pid);

    if(aux == NULL) {
        printf(ERROR"Nenhum utente encontrado com PID {%d}.", pid);
    }
    else {

        printf("UTENTE NOME{"MAG"%s"CRT"} PID{"MAG"%d"CRT"} PRIORIDADE{",aux->nome ,aux->pid);
        switch(aux->prioridade) {
            case 1: printf(RED);
            break;
            case 2: printf(YLW);
            break;
            case 3: printf(GRN);
            break;
            default:
            break;
        }
        printf("%d"CRT"} ESPECIALIDADE{"MAG"%s"CRT"} ESTADO{", aux->prioridade, aux->especialidade);
        if(aux->estado) printf(BLE"EM ATENDIMENTO"CRT"}\n\n");
        else printf(YLW"EM ESPERA"CRT"}\n");

        if(aux->estado == EM_ATENDIMENTO) {
            printf("INFORMACAO SOBRE A CONSULTA:\n");
            struct par * p = procurar_par(pid);
            internal_info_par(p);
        }
    }
}

void info_especialista(int pid) {
    struct especialista * aux = procurar_especialista(pid);

    if(aux == NULL) {
        printf(ERROR"Nenhum especialista encontrado com PID {%d}.", pid);
    }
    else {

        printf("ESPECIALISTA NOME{"MAG"%s"CRT"} PID{"MAG"%d"CRT"}",aux->nome ,aux->pid);
        printf("ESPECIALIDADE{"MAG"%s"CRT"} ESTADO{", aux->especialidade);
        if(aux->estado) printf(BLE"EM ATENDIMENTO"CRT"}\n\n");
        else printf(YLW"EM ESPERA"CRT"}\n");

        if(aux->estado == EM_ATENDIMENTO) {
            printf("INFORMACAO SOBRE A CONSULTA:\n");
            struct par * p = procurar_par(pid);
            internal_info_par(p);
        }
    }
}

void info_par(int id) {
    struct par * p = encontrar_par(id);
    if(p == NULL) {
        printf(ERROR "Nenhuma informacao sobre a consulta de id %d.\n", id);
    }
    else {
        internal_info_par(p);
    }
}

void internal_info_par(struct par * p) {
    if(p == NULL) return;
    printf("CONSULTA COM ID{"RED"%d"CRT"}\n", p->id);
    printf("UTENTE PID{"MAG"%d"CRT"} NOME{"BLE"%s"CRT"} EM ATENDIMENTO POR \n", p->utente->pid, p->utente->nome);
    printf("ESPECIALISTA PID{"MAG"%d"CRT"} NOME{"BLE"%s"CRT"}\n\n", p->especialista->pid, p->especialista->nome);

    time_t tempo;
    time(&tempo);

    printf("DURACAO{"MAG"%0.f"CRT"} SEGUNDOS.\n", difftime(tempo, p->duracao));
    fflush(stdout); //
}


void internal_remover_par(int id) {
    struct par * p_aux = encontrar_par(id);
    if(p_aux == NULL) {
        printf(ERROR "Nenhuma consulta com ID{"MAG"%d"CRT"}.\n", id);
    }
    else {
        p_aux->especialista->estado = false;
        p_aux->utente->estado = false;

        internal_remover_utente(p_aux->utente->pid);

        struct mensagem resposta;

        montar_pacote(&resposta,
        BALCAO_NOME, 
        p_aux->especialista->nome,
        BALCAO_ID, 
        p_aux->especialista->pid, 
        CONSULTA_ENCERRADA_REPLY, 
        strlen(CONSULTA_ENCERRADA_REPLY), 
        MENSAGEM_DESEMPARELHAR);
        
        if(enviar_pacote(&resposta) == false) {
            internal_remover_especialista(p_aux->especialista->pid);
        }

        if(p_aux->prev) {
            p_aux->prev->next = p_aux->next;
        }
        if(p_aux->next) {
            p_aux->next->prev = p_aux->prev;
        }
        if(p_aux->prev == 0) {
            lista_pares = p_aux->next;
        }

        free(p_aux);
    }
}


void inserir_par(struct par * p) {
    if(lista_pares == NULL) {
        lista_pares = p;
    }
    else {
        struct par * p_aux;

        for(p_aux = lista_pares; p_aux->next != NULL; p_aux = p_aux->next) {}
        
        p_aux->next = p;
        p->prev = p_aux;
    }
}

struct par * gerar_par(struct utente * u, struct especialista * e, int id) {
    struct par * p = (struct par *)malloc(sizeof(struct par));

    p->utente = u;
    p->especialista = e;

    p->next = 0;
    p->prev = 0;

    p->id = id;
    time(&p->duracao);

    return p;
}

void mostrar_pares() {
    PINFO("Lista de consultas:");
    if(lista_pares == NULL) {
        printf("Nenhuma consulta a decorrer.\n");
    }
    else  {
        struct par * p_aux;
        for(p_aux = lista_pares; p_aux != NULL; p_aux = p_aux->next) {
            internal_info_par(p_aux);
            printf("\n");
        }
    }
    printf(INFO"Encontram se a decorrer "BLE"%d"CRT" consultas no sistema.\n", contagem_pares());
}

int contagem_pares() {
    int contador = 0;

    struct par * aux;
    for(aux = lista_pares; aux != 0; aux = aux->next) {
        contador++;
    }

    return contador;
}

void libertar_consultas() {
    struct par * p_aux = lista_pares;

    while(p_aux) {
        internal_remover_par(p_aux->id);
        p_aux = lista_pares;
    }
    lista_pares = 0;
}

// testar
struct par * procurar_par(int pid) {
    if(lista_pares == NULL) return NULL;

    struct par * p_aux;
    for(p_aux = lista_pares; p_aux != NULL; p_aux = p_aux->next) {
        if(p_aux->especialista->pid == pid ||
            p_aux->utente->pid == pid) {
            return p_aux;
        }
    }
    return NULL;
}

void * freq(void * parms) {
    do {
        pthread_mutex_lock(&freq_lock);
        int internal_freq = frequencia;
        pthread_mutex_unlock(&freq_lock);

        sleep(internal_freq);

        mostrar_utentes();
        mostrar_especialistas();
        mostrar_pares();
        ask();
    } while(true);
    return NULL;
}

void mudar_freq(int nova_freq) {
    if(nova_freq <= 0) {
        PERROR("Valores invalidos para frequencia.");
    }
    else {
        pthread_mutex_lock(&freq_lock);
        frequencia = nova_freq;
        pthread_mutex_unlock(&freq_lock);
        PINFO("Frequencia de informacao alterada.");
    }
}

void mudar_freq_cmd(char * input) {
    if(contar_palavras(input) != 2)
    {
        PINFO("Utilize o seguinte formato para o comando:");
        PRINT(stdout, FORMAT("freq {N}"));
        return;
    }

    char delim[] = " \n";
    char * ptr = strtok(input, delim);
    
    int n = atoi(strtok(NULL, delim));

    mudar_freq(n);
}

void ask() {
    static time_t rawtime;
    static struct tm * timeinfo;

    time (&rawtime);
    timeinfo = localtime (&rawtime);
    printf(CMD MAG"administrador"CRT"@"CYN"MEDICALso"CRT" [%d:%d:%d]> ", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
    fflush(stdout);
}


void info_utente_cmd (char * input) {
    if(contar_palavras(input) != 2) {
        PINFO("Utilize o seguinte formato para o comando:");
        PRINT(stdout, FORMAT("utente {PID}"));
        return;
    }

    char delim[] = " \n";
    char * ptr = strtok(input, delim);
    
    int pid = atoi(strtok(NULL, delim));

    info_utente(pid);
}

void info_especialista_cmd (char * input) {
    if(contar_palavras(input) != 2) {
        PINFO("Utilize o seguinte formato para o comando:");
        PRINT(stdout, FORMAT("especialista {PID}"));
        return;
    }

    char delim[] = " \n";
    char * ptr = strtok(input, delim);
    
    int pid = atoi(strtok(NULL, delim));

    info_especialista(pid);
}

void info_consulta_cmd (char * input) {
    if(contar_palavras(input) != 2) {
        PINFO("Utilize o seguinte formato para o comando:");
        PRINT(stdout, FORMAT("consulta {ID}"));
        return;
    }

    char delim[] = " \n";
    char * ptr = strtok(input, delim);
    
    int id = atoi(strtok(NULL, delim));

    info_par(id);
}

void * heartbeat(void * parms) {
    do {
        sleep(1);
        pthread_mutex_lock(&heartbeat_lock);
        time_t tempo;
        time(&tempo);
        for(struct especialista * aux = lista_especialistas;
            aux != NULL;) {
                if(difftime(tempo, aux->TTL) >= 20.0) {
                    printf(INFO"TIMEOUT: ESPECIALISTA "MAG"%s"CRT" ["MAG"%d"CRT"]\n", aux->nome, aux->pid);
                    struct especialista * temp = aux->next;
                    internal_remover_especialista(aux->pid);
                    ask();
                    aux = temp;
                }
                else {
                    aux = aux->next;
                }
        }
        pthread_mutex_unlock(&heartbeat_lock);

    } while(true);
}