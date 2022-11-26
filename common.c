#include "common.h"
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <fcntl.h>
#include <unistd.h>

void montar_pacote(struct mensagem * pacote, 
        const char * nome_origem,
        const char * nome_destino,
        int pid_origem,
        int pid_destino, 
        const char * buffer, 
        int len, 
        int reserved) {

    memset(pacote,0,sizeof(struct mensagem));
    int tamanho = len;

    memcpy( pacote->informacao.origem.nome,  nome_origem, NOME_LEN-1);
    memcpy(pacote->informacao.destino.nome, nome_destino, NOME_LEN-1);

    pacote->informacao.origem.pid  =  pid_origem;
    pacote->informacao.destino.pid = pid_destino;

    pacote->informacao.reservado   = reserved;
    if(tamanho >= DATA_LEN) {
        tamanho = DATA_LEN - 1;
    }
    pacote->informacao.tamanho = tamanho;
    memcpy(pacote->conteudo, buffer, tamanho);
}

bool enviar_pacote(const struct mensagem * pacote) {
if(pacote == NULL) return true;

    char    reply_pipe[10];
    sprintf(reply_pipe, "%d", pacote->informacao.destino.pid);

    int exists = access(reply_pipe, F_OK);
    if(exists != 0) {
        //PERROR("Pipe alvo inacessivel.");
        return false;
    }

    int   reply_fd = open(reply_pipe, O_RDWR);
    write(reply_fd, pacote, sizeof(struct mensagem));
    close(reply_fd);

    return true;
}

void mostrar_pacote(const struct mensagem * pacote) {
    printf("De: "MAG"%s"CRT" ["MAG"%d"CRT"], Para: "MAG"%s"CRT" ["MAG"%d"CRT"]\n", 
        pacote->informacao.origem.nome,
        pacote->informacao.origem.pid,
        pacote->informacao.destino.nome,
        pacote->informacao.destino.pid);

    printf("Conteudo:\n \t"MAG">"CRT);
    fflush(stdout);
    fputs(pacote->conteudo, stdout);
    fflush(stdout);
}

bool check_pipes(const char * pipe_1, const char * pipe_2 ) {

    int exists = access(pipe_1, F_OK);
    if(exists != 0) {
        printf("%s inacessivel.", pipe_1);
        PRINT(stdout, FORMAT("Tente novamente mais tarde..."));
        return false;
    }
    printf(SUCCESS"%s acessivel.", pipe_1);
    
    exists = access(pipe_2, F_OK);
    if(exists == 0) {
        PERROR("Conflito interno de pipes.");
        PRINT(stdout, FORMAT("Tente novamente mais tarde..."));
        return false;
    }
    printf(SUCCESS "%s acessivel.\n", pipe_2);

    return true;
}