#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>

#include "common.h"


struct {
int balcao;
    char nome[NOME_LEN];
    int  pid;
    char pipe[10];
    int  fd;
} info;

struct {
bool estado;
    char nome[NOME_LEN];
    int  pid;
    char pipe[10];
    int  fd;
} info_par;

bool registar_par(const struct mensagem *);
void signal_pipe(int);
void heartbeat();
void sair();

void sair() {
    close(info_par.fd);
    close(info.fd);
    unlink(info.pipe);

    exit(0);
}

void heartbeat() {
    struct mensagem resposta = {0};

    montar_pacote(&resposta,
    info.nome,
    BALCAO_NOME,
    info.pid,
    BALCAO_ID,
    NULL,
    0, 
    MENSAGEM_HEARTBEAT);
    write(info.balcao, &resposta, sizeof(struct mensagem));
}

void signal_pipe(int signal)
{
    PERROR("Comunicacao com o sistema interrompida.");
    PRINT(stdout, FORMAT("A encerrar..."));
    sair();
}

bool registar_par(const struct mensagem * pacote) {
if(pacote == NULL) return false;

    sscanf( pacote->conteudo, "%d", &info_par.pid);
    sprintf(info_par.pipe,    "%d", info_par.pid);
    
    memcpy(info_par.nome, pacote->informacao.origem.nome, NOME_LEN);

    if(access(info.pipe, F_OK) != 0) {
        PERROR("Impossivel comunicar com o par."); //!
        info_par.estado = EM_ESPERA;
    }
    else {
        info_par.estado = EM_ATENDIMENTO;
        PSUCCESS("Emparelhado com sucesso.");

        printf(  "Informacao do par:\n");
        printf(  "Nome: "MAG"%s"CRT" PID: "MAG"%d"CRT"\n", info_par.nome, info_par.pid);
    }
    return info_par.estado;
}

int main(int argc, char ** argv)
{
    if(argc != 3) {
        PERROR("Inclua nome do especialista e especialidade na chamada do programa.");
        fprintf(stderr, "Exemplo: %s \"Duarte Mendes\" cardiologia\n", argv[0]);
        return PARMS_MISSING;
    }

    info_par.estado = EM_ESPERA;
    info.pid = getpid();
    sprintf(info.pipe,    "%d",     info.pid);
    memcpy( info.nome, argv[1], NOME_LEN - 1);


    printf(SUCCESS"Bem vindo(a), "MAG"%s"CRT"!\n", info.nome);
    printf(RED"P"GRN"I"BLE"D"YLW":"CRT" (%d)\n", getpid());

    signal(SIGPIPE, signal_pipe);

    if(check_pipes(PIPE_ESPECIALISTAS, info.pipe) == false) {
        return PIPE_MISSING;
    }

    //////////////////////////////////////
    info.balcao =  open(PIPE_ESPECIALISTAS, O_WRONLY);
    //
    mkfifo(info.pipe, 0600);
    info.fd =  open(info.pipe, O_RDWR);
    //////////////////////////////////////

    if(info.balcao == -1 || info.fd == -1) {
        PERROR("Erro interno, canais inacessiveis.");
        return FILE_ERROR;
    }

    struct mensagem pacote = {0};
    montar_pacote(&pacote, 
    info.nome,
    BALCAO_NOME,
    info.pid, 
    BALCAO_ID, 
    argv[2], 
    strlen(argv[2]), 
    MENSAGEM_REGISTO);

    if( write(info.balcao, &pacote, sizeof(struct mensagem)) 
        == sizeof(struct mensagem)) {
        PSUCCESS("A especialidade foi enviada para o balcao. Aguarde confirmacao...");
    }
    else {
        PERROR("Comunicacao com o sistema interrompida.");
        PRINT(stdout, FORMAT("Algo de inesperado sucedeu, tente novamente mais tarde..."));
        sair();
    }

    char    buffer[DATA_LEN] = {0};
    memset(&pacote, 0, sizeof(struct mensagem));

    read(info.fd, &pacote, sizeof(struct mensagem));


    if(pacote.informacao.reservado == MENSAGEM_FALHA) {
        mostrar_pacote(&pacote);
        PERROR("Falha no registo. Motivo:");

        sair();
    }
    PSUCCESS("Registo concluido com sucesso!");
    PINFO("Estamos em lista de espera. Aguarde resposta do balcao ou escreva "BLE"ajuda"CRT" para sair.");

    struct timeval heartbeat_timer;
    do {
        printf("\n>");
        fflush(stdout);

        fd_set    file_descriptors;
        FD_ZERO(&file_descriptors);

        FD_SET(STDIN_FILENO, &file_descriptors);
        FD_SET(info.fd,      &file_descriptors);

        heartbeat_timer.tv_sec = 10;
        heartbeat_timer.tv_usec = 0;

        int res = select(info.fd + 1, &file_descriptors, NULL, NULL, &heartbeat_timer);

        struct mensagem   pacote = {0};
        struct mensagem resposta = {0};

        memset(buffer, 0, DATA_LEN);

        if(res == 0) {
            heartbeat();
        }
        if(res > 0) {
            if(FD_ISSET(STDIN_FILENO, &file_descriptors)) { 
                fgets(buffer, DATA_LEN -1, stdin);
                if(info_par.estado == EM_ATENDIMENTO) {
                    if(strcmp(buffer, "sair\n") == 0) {
                        montar_pacote(&resposta,
                        info.nome,
                        BALCAO_NOME,
                        info.pid,
                        BALCAO_ID,
                        buffer,
                        strlen(buffer), 
                        MENSAGEM_SAIR_CONSULTA);
                        write(info.balcao, &resposta, sizeof(struct mensagem));

                        PINFO("Consulta terminada. Aguarde novo emparelhamento...");
                        printf("Pode sair a qualquer momento do sistema ao escrever "BLE"sair"CRT".\n");
                    }
                    else {
                        montar_pacote(&resposta, 
                        info.nome, 
                        info_par.nome,
                        info.pid, 
                        info_par.pid, 
                        buffer, 
                        strlen(buffer), 
                        MENSAGEM_TEXTO);
                        enviar_pacote(&resposta);

                        heartbeat();
                    }
                }
                else {
                    if(strcmp(buffer, "sair\n") == 0) {
                        montar_pacote(&resposta, 
                        info.nome,
                        BALCAO_NOME,
                        info.pid, 
                        BALCAO_ID, 
                        buffer, 
                        strlen(buffer), 
                        MENSAGEM_SAIR);
                        write(info.balcao, &resposta, sizeof(struct mensagem));

                        PINFO("Sistema notificado.");
                        sair();
                    }
                }
            }
            if(FD_ISSET(info.fd, &file_descriptors)) {
                int nbytes = read(info.fd, &pacote, sizeof(struct mensagem));
                heartbeat();

                if(nbytes == sizeof(struct mensagem)) { 
                    if(pacote.informacao.origem.pid == BALCAO_ID) {
                        mostrar_pacote(&pacote);

                        switch(pacote.informacao.reservado) {
                            case MENSAGEM_EMPARELHAR:
                            if(registar_par(&pacote)) {
                                printf("A consulta esta a decorrer. Escreva para falar com o especialista:\n");
                            }
                            break;
                            case MENSAGEM_DESEMPARELHAR:
                            info_par.estado = false;
                            info_par.pid = 0;
                            break;
                            case MENSAGEM_SAIR:
                            sair();
                            default:
                            break;
                        }
                    }
                    else if(info_par.estado == EM_ATENDIMENTO) {
                        if(pacote.informacao.origem.pid == info_par.pid) {
                            mostrar_pacote(&pacote);
                        }
                    }
                }
            }
        }
    }while(true);

    sair();
    return 0;
}