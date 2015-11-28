/***********************************************************/
/*                                                         */
/*     MAC0422 - Sistemas Operacionais                     */
/*     Professor: Daniel Batista                           */
/*                                                         */
/*     Marcos Yamazaki          7577622                    */
/*     Caio Lopes               7991187                    */
/*                                                         */
/*     Data: 10/Setembro/2015                              */
/*     ep1sh.c                                             */
/*                                                         */
/*     Shell Simples                                       */
/*                                                         */
/***********************************************************/

/*  COMPILAR: gcc ep1sh.c -o ep1sh -lreadline              */

#include <stdio.h> 
#include <string.h> 
#include <stdlib.h> 
#include <unistd.h> 
#include <readline/readline.h>
#include <readline/history.h>

/***********************************************************/
/*
/*  Executa os comandos digitados na linha de comando
/*  @_param1 - String onde esta o comando
/*  @_param2 - String do diretorio atual onde o comando 
/*             esta sendo executado
/*                                                         
/***********************************************************/
void runcmd (char *input, char* pasta_atual) {
    char* arg[32];         /* Array de string que sera passado como parametro em execv */
    char* pch;             /* String do comando divididos a cada espaco vazio ' '      */
    char bin[6] = "/bin/"; /* Para completar comandos do diretorio /bin                */
    int i = 0;

    pch = strtok (input," "); /* Split a string pelos espacos                          */
    
    if(pch[0] == '.') { /* Se o comando comeca com o ponto '.', isso significa que     */
                        /* o comando a ser executado esta dentro da pasta atual        */
        strcat(pasta_atual, ++pch );
        arg[i++] = pasta_atual;
    } 
    else if(pch[0] != '/') {
        strcat(bin, pch ); /* concatena a string /bin/ no comando a ser executado      */
        arg[i++] = bin;
    }
    else arg[i++] = pch; /* O comando nao precisa ser modificado, pois ele ja esta por completo */
    
    while (pch != NULL) {
        /* Enquanto tiver argumentos apos o comando, separa cada argumento em um espaco no array */
        pch = strtok (NULL, " ");
        arg[i++] = pch;
    }
    arg[i] = 0; /* Final dos argumentos */

    execv(arg[0], arg); /* Executa o binario arg[0], passando o pid desse processo para o novo processo */
                        /* Ja que esse processo ira 'morrer'                                            */                                                                                    
}

int main (int argc, char *argv[]) {
    char* input,                       /* comando na linha do shell                          */ 
          shell_prompt[1024];          /* linha a ser impressa no simulador do shell         */ 
    rl_bind_key('\t', rl_complete);    /* Pode-se usar a tecla TAB, para auto completar      */
    char* pasta_atual = getcwd(NULL, 1024); /* Guarda a string com o nome do diretorio atual */

    while (1) {
        pasta_atual = getcwd(NULL, 1024);
        snprintf(shell_prompt, sizeof(shell_prompt), "[%s] ", pasta_atual );

        input = readline(shell_prompt); /* faz a leitura dos comandos apos a quebra de linha      */
        add_history(input); /* Permite que o usuario tenha acesso aos comando previamentes usados */

        if(input[0] == 'c' && input[1] == 'd' && input[2] == ' ') chdir(input+3); /* Executa o comando cd                     */
                                                                                  /* levando em consideracao que o diretorio  */
                                                                                  /* sera escrito corretamente                */
        else {
            if(fork() == 0) runcmd(input, pasta_atual); /* Cria um processo filho para executar outros comandos     */
            else wait(NULL); /* Se executar aqui, eh o processo pai, que vai esperar pelo processo filho terminar   */
        }
        free(input);
    }
    return 1;
}