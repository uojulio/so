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
} info_par;

void signal_pipe(int);
bool check_pipes();
bool registar_par(const struct mensagem *);
void sair();

void sair() {
    close(info.balcao);
    close(info.fd);
    unlink(info.pipe);

    exit(0);
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
        // ele mostra o nome do balcao aqui, talvez re fazer no lado do balcao para nas mensagems
        // de emparelhamento ele meter o nome do par no campo de nome
        printf(  "Informacao do par:\n");
        printf(  "Nome: "MAG"%s"CRT" PID: "MAG"%d"CRT"\n", info_par.nome, info_par.pid);
    }
    return info_par.estado;
}


void signal_pipe(int signal)
{
    PERROR("Comunicacao com o sistema interrompida.");
    PRINT(stdout, FORMAT("A encerrar..."));
    sair();
}

int main(int argc, char ** argv)
{
    if(argc != 2) {
        PERROR("Inclua nome do utente na chamada do programa.");
        fprintf(stderr, "Exemplo: %s \"Joao Silva\"\n", argv[0]);
        return PARMS_MISSING;
    }

    info_par.estado = EM_ESPERA;
    info.pid = getpid();
    sprintf(info.pipe,    "%d",     info.pid);
    memcpy( info.nome, argv[1], NOME_LEN - 1);


    printf(SUCCESS"Bem vindo(a), "MAG"%s"CRT"!\n", info.nome);
    printf(RED"P"GRN"I"BLE"D"YLW":"CRT" (%d)\n", getpid());

    signal(SIGPIPE, signal_pipe);

    if(check_pipes(PIPE_UTENTES, info.pipe) == false) {
        return PIPE_MISSING;
    }


    //////////////////////////////////////
    info.balcao =  open(PIPE_UTENTES, O_WRONLY);  // BLOCKING
    //
    mkfifo(info.pipe, 0600);
    info.fd =  open(info.pipe, O_RDWR);           // NON-BLOCKING
    //////////////////////////////////////

    if(info.balcao == -1 || info.fd == -1) {
        PERROR("Erro interno, canais inacessiveis.");
        return FILE_ERROR;
    }

    printf("Por favor, indique os seus sintomas.\n"MAG">"CRT"");

    char  buffer [DATA_LEN];
    fgets(buffer, DATA_LEN -1, stdin);


    struct mensagem pacote = {0};
    montar_pacote(&pacote, 
    info.nome, 
    BALCAO_NOME, 
    info.pid, 
    BALCAO_ID, 
    buffer, 
    strlen(buffer), 
    MENSAGEM_REGISTO);

    int nbytes = write(info.balcao, &pacote, sizeof(struct mensagem));
    if( nbytes == sizeof(struct mensagem)) {
        PSUCCESS("Os sintomas foram enviados para o balcao. Aguarde confirmacao...");
    }
    else {
        PERROR("Comunicacao com o sistema interrompida.");
        PRINT(stdout, FORMAT("Algo de inesperado sucedeu, tente novamente mais tarde..."));
        sair();
    }

    struct mensagem resposta = {0};

    read(info.fd, &resposta, sizeof(struct mensagem));

    if(resposta.informacao.reservado == MENSAGEM_FALHA) {
        mostrar_pacote(&resposta);
        PERROR("Falha no registo.");

        sair();
    }
    PSUCCESS("Registo concluido com sucesso!");
    PINFO("Estamos em lista de espera. Aguarde resposta do balcao ou escreva "BLE"ajuda"CRT" para sair.");
    
    do {
        printf("\n>");
        fflush(stdout);

        fd_set    file_descriptors;
        FD_ZERO(&file_descriptors);

        FD_SET(STDIN_FILENO, &file_descriptors);
        FD_SET(info.fd,      &file_descriptors);

        int res = select(info.fd + 1, &file_descriptors, NULL, NULL, NULL);

        struct mensagem   pacote; // mensagem recebida
        struct mensagem resposta; // mensagem a enviar

        memset(  &pacote, 0, sizeof(struct mensagem));
        memset(&resposta, 0, sizeof(struct mensagem));

        if(res > 0) {
            if(FD_ISSET(STDIN_FILENO, &file_descriptors)) { 
                char buffer[DATA_LEN];
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

                        sair();
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
                        // ver caso de falha
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

                        sair();
                    }
                }
            }
            if(FD_ISSET(info.fd, &file_descriptors)) {
                int nbytes = read(info.fd, &pacote, sizeof(struct mensagem));

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
                            case MENSAGEM_SAIR:
                            sair();
                            default:
                            break;
                        }
                    }
                    else if(info_par.estado == true) {
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