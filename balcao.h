#ifndef BALCAO_H
#define BALCAO_H

#include <time.h>
#include <pthread.h>

#include "common.h"


// CONSTANTES SIMBÓLICAS DO SISTEMA
#define LIMITE_ESPERA 5
//
#define CLIENTES_ENV "MAXCLIENTES"
#define  MEDICOS_ENV  "MAXMEDICOS"


//  VALOR DE ESPERA DE HEARTBEAT
#define HEARTBEAT_TTL (20.0)

// GLOBAIS CAPACIDADE
static const char * DEFAULT_MAXCLIENTES = "25";
static const char * DEFAULT_MAXMEDICOS  = "25";
//
extern int MAXCLIENTES;
extern int MAXMEDICOS;


//  FD PIPES PRINCIPAIS
extern int       utentes_fd;
extern int especialistas_fd; 


//  UNNAMED PIPES
extern int balcao_para_classificador[2];
extern int classificador_para_balcao[2];


// VARIAVEIS DE FREQUENCIA
extern int frequencia;
//
extern pthread_mutex_t freq_lock;
extern pthread_t     freq_thread;


//  LISTA DE ESPECIALIDADES
static const char * ESPECIALIDADES[] = 
{
    "oftalmologia", 
    "neurologia", 
    "estomatologia", 
    "ortopedia", 
    "geral"
};




// ESTRUTURAS DE DADOS DO SISTEMA
struct utente {
    int pid;

    char nome[NOME_LEN];

    struct utente * prev;
    struct utente * next;

    int prioridade;

    const char * especialidade;

    bool estado;
};
//
struct especialista {
    int pid;

    char nome[NOME_LEN];

    struct especialista * prev;
    struct especialista * next;

    const char * especialidade;

    bool estado;
    time_t TTL;
};
//
struct par {

    struct utente * utente;
    struct especialista * especialista;

    struct par * prev;
    struct par * next;

    int id;

    time_t duracao;
};
//
struct comando
{
    const char * nome;
    const char * descricao;

    void (*func)(char *);
};


//  LISTAS LIGADAS
static struct utente *             lista_utentes = 0;
static struct especialista * lista_especialistas = 0;
static struct par *                  lista_pares = 0;


//  FUNÇÕES DE SISTEMA
bool iniciar_balcao();
void encerrar_balcao();


//  FUNÇÕES DO CLASSIFICADOR
void iniciar_classificador(int);
void encerrar_classificador();
//
void obtem_classificacao(struct utente *, const char *);
//
void signal_classificador(int);


//  FUNÇÕES AUXILIARES STRINGS
void parse_str(char *);
int contar_palavras(char *);


//  FUNÇÕES DE COMANDOS
void comandos();
//
void verificar_comando(char *);


//  FUNÇÕES PARA REMOVER OBJETOS
void       internal_remover_utente(int);
void internal_remover_especialista(int);
void          internal_remover_par(int);


//  FUNÇÕES PARA INSERIR NAS LISTAS
void             inserir_utente(struct utente *);
void inserir_especialista(struct especialista *);
void                   inserir_par(struct par *);


//  FUNÇÕES PARA REGISTO
bool           adicionar_utente(struct mensagem *, char *);
bool     adicionar_especialista(struct mensagem *, char *);
void adicionar_par(struct utente *, struct especialista *);
//
struct utente *             criar_utente(struct mensagem *);
struct especialista * criar_especialista(struct mensagem *);
//
bool             validar_utente(struct utente *, char *);
bool validar_especialista(struct especialista *, char *);
//
struct utente *        gerar_utente(int, const char *, int, const char *);
struct especialista * gerar_especialista(int, const char *, const char *);
struct par *       gerar_par(struct utente *, struct especialista *, int);


//  FUNÇÕES PARA VISUALIZAR
void       mostrar_utentes();
void mostrar_especialistas();
void         mostrar_pares();
//
void info_utente(int);
void info_especialista(int);
void info_par(int);
//
void internal_info_par(struct par *);


//  FUNÇÃO DE EMPARELHAMENTO
void emparelhar();


//  FUNÇÕES DE CONTAGEM
int       contagem_utentes();
int contagem_especialistas();
int         contagem_pares();
//
int contagem_utentes_especialidade(const char *);


//  FUNÇÕES DE LIMPEZA
void       libertar_utentes();
void libertar_especialistas();
void     libertar_consultas();


//  FUNÇÕES DE PESQUISA NO SISTEMA
struct utente *             procurar_utente(int);
struct especialista * procurar_especialista(int);
struct par *                   procurar_par(int);
//
struct par * encontrar_par(int);


//  FUNÇÕES DE FREQUÊNCIA
void * freq(void *);
//
void mudar_freq(int);

//  FUNÇÃO DE HEARTBEAT
void * heartbeat(void *);

//  FUNÇÃO PARA TERMINAL
void ask();

//  FUNÇÕES PARA COMANDOS
void comandos_cmd               (char *);
//
void mostrar_utentes_cmd        (char *);
void mostrar_especialistas_cmd  (char *);
void mostrar_pares_cmd          (char *);
//
void info_utente_cmd            (char *);
void info_especialista_cmd      (char *);
void info_consulta_cmd          (char *);
//
void remover_utente_cmd         (char *);
void remover_especialista_cmd   (char *);
void remover_par_cmd            (char *);
//
void limpar_terminal_cmd        (char *);
void encerrar_balcao_cmd        (char *);
//
void mudar_freq_cmd             (char *);


//  LISTA DE COMANDOS
static const struct comando comandoss[] = {
    {"ajuda", "mostra a lista de comandos permitidos",                                   &comandos_cmd},
    {"utentes", "lista os utentes registados no sistema",                                &mostrar_utentes_cmd},
    {"especialistas", "lista os especialistas registados no sistema",                    &mostrar_especialistas_cmd},
    {"consultas", "lista as consultas a decorrer no momento",                            &mostrar_pares_cmd},
    {"utente {PID}", "mostra informacoes sobre o utente com {PID} correspondente",             &info_utente_cmd},
    {"especialista {PID}", "mostra informacoes sobre o especialista com {PID} correspondente", &info_especialista_cmd},
    {"consulta {ID}", "mostra informacoes sobre a consulta com {ID} correspondente",          &info_consulta_cmd},
    {"delut {PID}", "elimina um utente registado no sistema com {PID} correspondente",         &remover_utente_cmd},
    {"delesp {PID}", "elimina um especialista registado no sistema com {PID} correspondente",  &remover_especialista_cmd},
    {"delcon {ID}", "elimina uma consulta a decorrer com {ID} correspondente",                &remover_par_cmd},
    {"clear", "limpa a terminal",                                                        &limpar_terminal_cmd},
    {"encerra", "encerra o balcao",                                                      &encerrar_balcao_cmd},
    {"freq N", "altera a frequencia em  N segundos com que mostramos informacoes uteis",      &mudar_freq_cmd}
};


// MACRO PARA FUNÇÕES DE MENSAGEM
#define MENSAGEM(X,...) X##_mensagem(__VA_ARGS__)
void       utentes_mensagem(struct mensagem *);
void especialistas_mensagem(struct mensagem *);


// MENSAGENS PADRÃO PARA RESPOSTA
#define         REGISTO_LOTACAO_REPLY "Lotacao atingida. Por favor, tente mais tarde."
#define         REGISTO_SUCESSO_REPLY "Registo concluido. Por favor aguarde emparelhamento."
#define    REGISTO_INVALIDO_PID_REPLY "Falha no registo. PID duplicado."
#define REGISTO_INVALIDO_LIMITE_REPLY "Falha no registo. Limite para categoria atingido."
#define      CONSULTA_ENCERRADA_REPLY "Consulta terminada."
#define                    SAIR_REPLY "Removido do sistema."

#endif