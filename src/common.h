#if    !COMMON_H
#define COMMON_H

#include "colors.h"

#include <stdbool.h>

#define PRINT(...)  fprintf(__VA_ARGS__)
#define FORMAT(X)   X "\n"
#define PERROR(X)   PRINT(stderr, FORMAT(ERROR X))
#define PINFO(X)    PRINT(stdout, FORMAT(INFO X))
#define PSUCCESS(X) PRINT(stdout, FORMAT(SUCCESS X))
#define PCMD(X)     PRINT(stdout, FORMAT(CMD X))


#define       PIPE_UTENTES  "MEDICALso_UTENTES"
#define PIPE_ESPECIALISTAS  "MEDICALso_ESPECIALISTAS"

#define BALCAO_NOME "MEDICALso"
#define BALCAO_ID 0

//  RETURN VALUES
#define PARMS_MISSING   1
#define  PIPE_MISSING   2
#define    FILE_ERROR   3
#define    UNEXPECTED   9

//  estrutura das mensagens enviadas pelo named pipe
#define NOME_LEN    64
#define DATA_LEN    256
struct mensagem {
    struct {
        struct {
            int pid;
            char nome[NOME_LEN];
        } origem;
        struct {
            int pid;
            char nome[NOME_LEN];
        } destino;

        int tamanho;
        int reservado;
    } informacao;

    char conteudo[DATA_LEN];
};

#define       MENSAGEM_REGISTO 0
#define       MENSAGEM_SUCESSO 1
#define         MENSAGEM_FALHA 2
#define          MENSAGEM_SAIR 3
#define    MENSAGEM_EMPARELHAR 4
#define MENSAGEM_DESEMPARELHAR 5
#define         MENSAGEM_TEXTO 6
#define MENSAGEM_SAIR_CONSULTA 7
#define     MENSAGEM_HEARTBEAT 8

void montar_pacote(struct mensagem *, const char *,const char *, int, int, const char *, int, int);
bool enviar_pacote(const struct mensagem *);
void mostrar_pacote(const struct mensagem *);
//
bool check_pipes(const char *, const char *);

#define EM_ESPERA      false
#define EM_ATENDIMENTO true

#endif