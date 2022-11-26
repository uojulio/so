#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "balcao.h"

int main() {

    printf(CLR);
    fflush(stdout);

    PSUCCESS("Bem vindo ao sistema MEDICALso!");
    if(iniciar_balcao() == false) {
        return 1;
    }

    printf(INFO"A trabalhar com limite de "MAG"%d"CRT" utentes e "MAG"%d"CRT" especialistas.\n", MAXCLIENTES, MAXMEDICOS);

    struct mensagem data = {0};
    char cmd[DATA_LEN];

    printf("\nIntroduza comandos para interagir com o sistema, ou escreva "BLE"ajuda"CRT" para obter a lista de comandos suportados.\n");

    do {
        memset(&data,0, sizeof(struct mensagem));
        ask();

        fd_set file_descriptors;
        FD_ZERO(&file_descriptors);

        FD_SET(    STDIN_FILENO, &file_descriptors);
        FD_SET(      utentes_fd, &file_descriptors);
        FD_SET(especialistas_fd, &file_descriptors);

        int res = select(especialistas_fd + 1, &file_descriptors, NULL, NULL, NULL);

        if(res > 0) {
            if(FD_ISSET(STDIN_FILENO, &file_descriptors)) {
                fgets(cmd, DATA_LEN - 1, stdin);
                parse_str(cmd);
                verificar_comando(cmd);
                if(strcmp(cmd, "encerra") == 0) break;
            }
            if(FD_ISSET(utentes_fd, &file_descriptors)) {
                int nbytes = read(utentes_fd, &data, sizeof data);
                if(nbytes == sizeof(struct mensagem)) {
                    MENSAGEM(utentes, &data);
                }
            }
            if(FD_ISSET(especialistas_fd, &file_descriptors)) {
                int nbytes = read(especialistas_fd, &data, sizeof data);
                if(nbytes == sizeof(struct mensagem)) {
                    MENSAGEM(especialistas, &data);
                }
            } 
        }
    } while(1);

    return 0;
}
