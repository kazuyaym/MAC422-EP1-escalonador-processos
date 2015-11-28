/***********************************************************/
/*                                                         */
/*     MAC0422 - Sistemas Operacionais                     */
/*     Professor: Daniel Batista                           */
/*                                                         */
/*     Marcos Yamazaki          7577622                    */
/*     Caio Lopes               7991187                    */
/*                                                         */
/*     Data: 10/Setembro/2015                              */
/*     ep1.c                                               */
/*                                                         */
/*     Escalonador de processos                            */
/*                                                         */
/***********************************************************/

/****************************************************************************************************************

    -------> FALTA FAZER <-------

    - Escalonadosres complicados! 
        - Round Robin
        - Com prioridade (round robin mais complicado)
        - Com deadline   (parecido com o anterior, so que a prioridade eh aquele que precisa terminar mais rapido)

*/

/* COMPILAR: gcc ep1.c -o ep1 -lpthread                               */
/* @_param1 : numero do escalonador que sera usado, inteiro de 1 a 6  */
/* @_param2 : arquivo trace de entrada                                */
/* @_param3 : nome do arquivo de saida                                */
/* @_param4 : (opicional) d, debbug                                   */

#include <stdio.h>  
#include <stdlib.h>  
#include <unistd.h> 
#include <semaphore.h>
#include <time.h>

#define TEMPO_QUANTUM 2.0

FILE *trace, *output;

struct processo *proc;         /* Array de struct com todos os processos           */
struct processo *init = NULL;  /* Inicio da lista de ligada, em ordem de chagada   */
struct processo *final = NULL; /* Final da lista ligada                            */

sem_t mutex[128];              /* Semaforo de todos os processos                   */
sem_t sem_nucleo;              /* Semaforo para mudar o valor das variaveis da CPU */

int CPUs[32];                  /* Guarda o id do processo que esta rodando em cada CPU      */
int nucleos_livres;            /* Numero de CPUs livres, para poder executar algum processo */
int processos_em_espera;       /* Quantos processos estao em estado de espera, pronto para serem excutados, porem pausados */ 
int numero_de_processos;       /* Numero total de processos que serao simulados             */
int numero_de_mudancas;        /* Numero de mudanca de contexto, quando o simulador pausa um processo para rodar outro */
int processo_terminou;         /* Numero de processos que terminaram de ser executado       */
int debbug;

clock_t tempoInicial, tempoFinal; /* Tempos para serem medidos do escalonador inteiro */
double tempoGasto;

typedef struct processo {
    pthread_t thread_id; /* Id da thread que sera criada                           */
    int id;
    int estado;          /* ESTADO DA THREAD (1-pausado/2-executando/3-executado)  */
    int CPU;             /* CPU que o processo esta sendo rodado atualmente        */ 
    float  quantum;      /* Tempo em que o processa chega no sistema               */

    float  t0;           /* Tempo em que o processa chega no sistema               */
    float  dt;           /* Tempo real da CPU que sera simulado esse processo      */
    float  tf;           /* Tempo real em que o processo terminou de rodar na CPU  */
    float  deadline;     /* Tempo maximo para termino do processo                  */
    float  remain;       /* Tempo restante para terminar                           */
    char   nome[128];    /* Nome do processo                                       */
    int    prioridade;   /* Prioridade do processo, -20 a 19                       */
    struct processo *next;      /* Lista ligada em ordem de chegada                */   
    struct processo *prox_fila; /* Lista ligada para o proximo a ser executado     */         
} PROCESSO;


/********************************************************************************************/
/*
/*    Todos os processos vao rodar essa funcao, que tem um looping basicamente conferindo
/* o tempo todo quando tempo ja se passou desde que ela comecou a rodar, quando ja deu 
/* tempo suficiente, ela para de rodar, ou ela fica pausada caso o escalonador tenha pausado
/*                                                         
/********************************************************************************************/
void *executaProcesso(void *arg){
    PROCESSO *tinfo = arg; 

    clock_t tempoInicialT,         /* Tempo inicial desse processo */
            tempoFinalT;           /* Tempo final desse processo   */  
    double  tempoExecutadoT = 0,   /* Tempo executando/gastando tempo real da CPU */
            tempoGastoT     = 0,   /* Tempo gastos entre as mudancas de contexto  */
            remaning;

    if(debbug) fprintf(stderr, "Processo da linha %d do trace, cujo nome eh %s, chegou no sistema\n", tinfo->id+1, tinfo->nome);
    while(1) {
        if(tinfo->estado == 1 ) {           /* O processo esta pausado! */
            tempoExecutadoT += tempoGastoT; /* Acumula o tempo que ja foi executado previamente */
            tempoGastoT = 0.0;              /* reseta o tempo, entre as pausas                  */
            sem_wait(&mutex[tinfo->id]);  
            if(debbug) fprintf(stderr, "Processo: \"%s\" esta usando a CPU %d\n", tinfo->nome, tinfo->CPU+1);
            tempoInicialT = clock();        /* Apos despausar, comeca a contar o tempo */
            tinfo->estado = 2;              /* Estado do processo: EXECUTANDO          */
        }
        tempoFinalT = clock();
        tempoGastoT = (double)(tempoFinalT-tempoInicialT)/(double)CLOCKS_PER_SEC; 
        remaning = tinfo->dt - (tempoExecutadoT + tempoGastoT);
        tinfo->remain = remaning;                             /* Guarda a todo o momento o tempo restante de execucao dessa thread */
        if(tempoExecutadoT + tempoGastoT >= tinfo->dt) break; /* Caso esse processo ja foi executado o tempo suficiente, termina   */
    }
                       
    tinfo->estado = 3;                      /* Thread, ja conseguiu rodar tudo que tinha que fazer */
    tempoFinalT = clock();                  /* Para de contar o tempo                              */ 
    tinfo->tf = (double)(tempoFinalT-tempoInicial)/(double)CLOCKS_PER_SEC; /* Compara o tempo inicial dessa execucao, */
                                                                           /* com o tempo do escalonador total        */ 
    sem_wait(&sem_nucleo);
        if(debbug) fprintf(stderr, "Processo: \"%s\" terminou!\n\tCom valores tf=%.1f e tr=%.1f\n\tLiberando a CPU %d\n", tinfo->nome, tinfo->tf, tinfo->tf - tinfo->t0, tinfo->CPU);
        nucleos_livres++;         /* Como terminou de rodar, agora temos mais uma CPU livre para rodar outro processo */
        CPUs[tinfo->CPU] = -1;    /* Coloca como -1 (CPU livre), na CPU em que estava sendo executada esse processo   */
        processo_terminou++;
        fprintf(output, "%s %.1f %.1f\n", tinfo->nome, tinfo->tf, tinfo->tf - tinfo->t0);
    sem_post(&sem_nucleo);

    return NULL;
}

/********************************************************************************************/
/*
/*                                                         
/********************************************************************************************/
void *executaProcesso_ROUNDROBIN(void *arg){
    PROCESSO *tinfo = arg; 

    clock_t tempoInicialT,         
            tempoFinalT;            
    double  tempoExecutadoT = 0,
            tempoGastoT     = 0, 
            remaning;

    while(1) {
        if(tinfo->estado == 1 ) {      
            tempoExecutadoT += tempoGastoT; 
            tempoGastoT = 0.0;             
            sem_wait(&mutex[tinfo->id]);  
            tempoInicialT = clock();     
            tinfo->estado = 2;           
        }
        tempoFinalT = clock();
        tempoGastoT = (double)(tempoFinalT-tempoInicialT)/(double)CLOCKS_PER_SEC; 
        remaning = tinfo->dt - (tempoExecutadoT + tempoGastoT);
        tinfo->remain = remaning;                             
        if(tempoExecutadoT + tempoGastoT >= tinfo->dt) break;
    }  
    tinfo->estado = 3;                     
    tempoFinalT = clock();                 
    tinfo->tf = (double)(tempoFinalT-tempoInicial)/(double)CLOCKS_PER_SEC; 
   
    sem_wait(&sem_nucleo);
        nucleos_livres++;         
        CPUs[tinfo->CPU] = -1;    
        processo_terminou++;
        fprintf(output, "%s %.1f %.1f\n", tinfo->nome, tinfo->tf, tinfo->tf - tinfo->t0);
    sem_post(&sem_nucleo);

    return NULL;
}

/*
/*    Funcao que verifica se ha algum nucleo 
/*  livre, e se tem processos na fila
/*                                                         
/*****************************************************************************/
struct processo* verifica_nucleoLivre_e_processosNaFila(struct processo *fila){
    int i; 

    while(nucleos_livres > 0 && processos_em_espera > 0) {
        /* Se temos algum nucleo para executar um processo, e tem algum processo em espera */
        /* executaremos aquele que chegou por primeiro                                     */
        sem_wait(&sem_nucleo);
            nucleos_livres--; /* Sera usado uma CPU */

            i = 0;                       /*   Nesta parte o escalonador procura o primeiro    */
            while(CPUs[i] != -1) i++;    /* CPU livre, e com certeza havera uma, essa tbm nao */
            CPUs[i] = fila->id;          /* vai utilizar um nucleo que nao existe, pois ja    */
            fila->CPU = i;               /* foi verificado isso anteriormente                 */

            sem_post(&mutex[fila->id]);
            fila = fila->prox_fila; /* Pega agora, para numa proxima rodada dessa funcao, o proximo da fila */
            processos_em_espera--;  /* Tirou um processo da lista de espera */
        sem_post(&sem_nucleo);
    }
    return fila;
}

/*
/*   Inicializa as threads quando 
/* esse processo chegar ao CPU                                
/*                         
/****************************************/
int criaProcesso (struct processo *lista){

    if ( pthread_create(&lista->thread_id, NULL, &executaProcesso, lista) ) {
        printf("Não foi possível criar a thread %d \n", lista->id);
        return EXIT_FAILURE;
    }
    lista->prox_fila = NULL;
    lista->estado = 1;
    lista->remain = lista->dt;
    processos_em_espera++;

    return EXIT_SUCCESS;
}

/*
/*   Inicializa as threads quando 
/* esse processo chegar ao CPU                                
/*                         
/****************************************/
int criaProcessoRR (struct processo *lista){
    if ( pthread_create(&lista->thread_id, NULL, &executaProcesso_ROUNDROBIN, lista) ) {
        printf("Não foi possível criar a thread %d \n", lista->id);
        return EXIT_FAILURE;
    }
    lista->prox_fila = NULL;
    lista->estado  = 1;
    lista->remain  = lista->dt;
    lista->quantum = 2.0;
    processos_em_espera++;

    return EXIT_SUCCESS;
}

/*_________________________________________________________________________________________________________
/*
/*                                                                                           ESCALONADORES 
/*_________________________________________________________________________________________________________
*/
/*-----------------------------------------------*/
/*                                               */
/*   FIRST-COME FIRST-SERVED                     */
/*                                               */
/*   O primeiro processo que chegar na           */
/*   CPU, eh o primeiro que sera executado       */
/*   sem mudancas de contexto                    */
/*                                               */
/*-----------------------------------------------*/
int fcfs() { 
    int    i = 0;
    struct processo *lista = init; /* Ponteiro para percorrer a lista ligada dos processos        */
    struct processo *fila  = NULL; /* Ponteiro para percorrer a lista ligadas da fila de execucao */
    struct processo *aux   = NULL; /* Ponteiro auxiliar, para manipulacao                         */

    processos_em_espera = 0; /* Reseta os valores */
    processo_terminou   = 0;
    numero_de_mudancas  = 0;
    tempoGasto          = 0;

    tempoInicial = clock();  /* Instante zero da simulacao, neste modo FCFS */

    while(processo_terminou < numero_de_processos) { /* Enquanto tem processos sendo executados, ou esperando */
        if(lista && tempoGasto >= lista->t0) {
            /* Caso ainda tenha algum processo que vai chegar, verifica se o processo chegou na CPU */
            /* Se chegou, criaremos seu processo, que no comeco estara em estado de espera          */
            /* Podemos simplismente ver o primeiro da fila, pois o arquivo trace esta em ordem nao decrescente */
            criaProcesso(lista); /* Cria a sua thread para a simulacao */

            /* Vai ordenando a fila para execucao */
            if(processos_em_espera == 1) fila = lista;
            else aux->prox_fila = lista;
            aux = lista;
            lista = lista->next;
        }
        fila = verifica_nucleoLivre_e_processosNaFila(fila); /* procura se algum processo pode rodar */

        /* A todo momento calculamos o tempo em segundo que ja foi percorrido */
        tempoFinal = clock();
        tempoGasto = (double)(tempoFinal-tempoInicial)/(double)CLOCKS_PER_SEC;  
    }

    /* esperando todas as threads terminarem */
    for (lista = init; lista; lista = lista->next) pthread_join(lista->thread_id, NULL);

    return EXIT_SUCCESS;
}

/*-----------------------------------------------*/
/*                                               */
/*   SHORTEST JOB FIRST                          */
/*                                               */
/*   Sem pausas, ou mudancas de contexto         */
/*   o proximo processo a ser liberado para      */
/*   execucao, sera aquele de menor tempo        */
/*                                               */
/*-----------------------------------------------*/
int sjf() {
    int i = 0;
    struct processo *lista = init; 
    struct processo *fila  = NULL;
    struct processo *aux   = NULL;

    processos_em_espera = 0;
    numero_de_mudancas  = 0; 
    processo_terminou   = 0;
    tempoGasto          = 0;

    tempoInicial = clock();       
    while(processo_terminou < numero_de_processos) {
        while(lista && tempoGasto >= lista->t0) {
            criaProcesso(lista);
            /*----------------------------------------------ORDENA A FILA DE ACORDO COM O MENOR PROCESSO ANTES   */
            if(processos_em_espera == 1) aux = fila = lista;
            else { /* fazer a lista dos mais curtos, daqueles que ja estao esperando ! */
                if(fila->dt > lista->dt) { /* coloca em primeiro lugar ! */
                    lista->prox_fila = fila;
                    aux = fila = lista;
                } else { /* Acha a posicao certa! */
                    while(aux->prox_fila && aux->prox_fila->dt <= lista->dt) aux = aux->prox_fila;
                    lista->prox_fila = aux->prox_fila;
                    aux->prox_fila = lista;
                    aux = fila;
                }
            }
            /*----------------------------------------------------------------------------------------------------*/
            lista = lista->next;
        }
        fila = verifica_nucleoLivre_e_processosNaFila(fila);
        tempoFinal = clock();
        tempoGasto = (double)(tempoFinal-tempoInicial)/(double)CLOCKS_PER_SEC;  
    }
    for (lista = init; lista; lista = lista->next) pthread_join(lista->thread_id, NULL);
    return EXIT_SUCCESS;
}

/*-----------------------------------------------*/
/*                                               */
/*   SHORTEST REMAINING TIME NEXT                */
/*                                               */
/*   Sem pausas, ou mudancas de contexto         */
/*   o proximo processo a ser liberado para      */
/*   execucao, sera aquele de menor tempo        */
/*                                               */
/*-----------------------------------------------*/
int srtn() {
    int i = 0;
    int id_maior;
    int cpu_troca_contexto;

    struct processo *lista = init;
    struct processo *fila  = NULL;
    struct processo *aux   = NULL;

    processos_em_espera = 0;
    numero_de_mudancas  = 0;
    processo_terminou   = 0;
    tempoGasto          = 0;

    tempoInicial = clock();
    while(processo_terminou < numero_de_processos) {
        while(lista && tempoGasto >= lista->t0) {
            criaProcesso(lista);

            if(processos_em_espera == 1) aux = fila = lista;
            else { /* fazer a lista dos mais curtos, daqueles que ja estao esperando ! */
                if(fila->remain > lista->remain) { /* coloca em primeiro lugar ! */
                    lista->prox_fila = fila;
                    aux = fila = lista;
                } else { /* Acha a posicao certa! */
                    while(aux->prox_fila && aux->prox_fila->remain <= lista->remain) aux = aux->prox_fila;
                    lista->prox_fila = aux->prox_fila;
                    aux->prox_fila = lista;
                    aux = fila;
                }
            }
            /*------------------------------------------------------------------VERIFICA SE DEVE PAUSAR PROCESSO EM ANDAMENTO    */
            if(nucleos_livres > 0) { /* A CPU, possui nucleos livres para processos  */ 
                                     /* rodarem, nao precisa fazer troca de contexto */
                lista = lista->next;
                break;
            }
            else { /* verifica todos os processos rodando para ver se acha o maior dos de tempo maior restantes */
                id_maior = CPUs[0];
                cpu_troca_contexto = 0;
                i = 1;
                while(CPUs[i] != -1) {
                    if(proc[CPUs[i]].remain > proc[id_maior].remain) {
                        id_maior = CPUs[i];
                        cpu_troca_contexto = i;
                    }
                    i++;
                }
            }
            /* Aqui ele achou o processo que deve ser trocado, caso o */
            /* processo que acabou de chegar tenha um tempo menor     */
            if(lista->remain < proc[id_maior].remain) { 
            /* Pausa o processo id_maior, se ele ainda tiver que        */
            /* rodar por mais tempo que o processo que acabou de chegar */
                sem_wait(&sem_nucleo);
                    if(debbug) fprintf(stderr, "Processo \"%s\" foi pausado, liberando a CPU %d\n\tPara troca de contexto\n", proc[id_maior].nome, cpu_troca_contexto+1);
                    proc[id_maior].estado = 1;            /* Pausa o processo */
                    CPUs[cpu_troca_contexto] = lista->id; /* Coloca o novo processo na CPU */
                    lista->CPU = cpu_troca_contexto;      /* No processo, coloque a CPU em que ela esta sendo executada */

                    aux = fila = &proc[id_maior];         /* Coloca em primeiro da fila, aquele processo que foi pausado */
                    fila->prox_fila = lista->prox_fila;   /* Adiciona normalmente o restante da fila                     */
                    lista->prox_fila = NULL;              /* Tira esse processo da fila, ja que sera executada           */
                    sem_post(&mutex[lista->id]);          /* Despausa, o processo que acabou de chegar                   */
                sem_post(&sem_nucleo);
                numero_de_mudancas++;
            }
            lista = lista->next;
        }
        fila = verifica_nucleoLivre_e_processosNaFila(fila);
        tempoFinal = clock();
        tempoGasto = (double)(tempoFinal-tempoInicial)/(double)CLOCKS_PER_SEC;  
    }
    for (lista = init; lista; lista = lista->next) pthread_join(lista->thread_id, NULL);
    return EXIT_SUCCESS;
}

/*-----------------------------------------------*/
/*                                               */
/*   ESCALONAMENTO POR PRIORIDADE                */
/*                                               */
/*   codigo meio que errado, sem ciclo           */
/*                                               */
/*-----------------------------------------------*/
int escalonamentoPrioridade() {
    int i = 0;
    int id_maior;
    int cpu_troca_contexto;

    struct processo *lista = init;
    struct processo *fila  = NULL;
    struct processo *aux   = NULL;

    processos_em_espera = 0; 
    numero_de_mudancas  = 0;
    processo_terminou   = 0;
    tempoGasto          = 0;

    tempoInicial = clock();  
    while(processo_terminou < numero_de_processos) {
        while(lista && tempoGasto >= lista->t0) {
            criaProcesso(lista);

            if(processos_em_espera == 1) aux = fila = lista;
            else { /* fazer a lista dos mais prioritarios, daqueles que ja estao esperando ! */
                if(lista->prioridade < fila->prioridade) { /* coloca em primeiro lugar ! */
                    lista->prox_fila = fila;
                    aux = fila = lista;
                } else { /* Acha a posicao certa! */
                    while(aux->prox_fila && aux->prox_fila->prioridade <= lista->prioridade) aux = aux->prox_fila;
                    lista->prox_fila = aux->prox_fila;
                    aux->prox_fila = lista;
                    aux = fila;
                }
            }

            /* Fazer igual ao anterior, caso algum que esteja rodando tenha menos prioridade que o atual, pausar! */
            ////////////////////////////////////////////////////////////////////////////////////////////////////////

            if(nucleos_livres > 0) {
                lista = lista->next;
                break;
            }
            else { /* verifica todos os processos rodando para ver se acha o maior dos de prioridade menor restantes */
                id_maior = CPUs[0];
                cpu_troca_contexto = 0;
                i = 1;

                while(CPUs[i] != -1) {
                    if(proc[CPUs[i]].prioridade > proc[id_maior].prioridade) {
                        id_maior = CPUs[i];
                        cpu_troca_contexto = i;
                    }
                    i++;
                }
            }
            /* aqui ele achou o processo que deve ser trocado, caso o processo que acabou de chegar tenha um tempo menor */
            if(lista->prioridade < proc[id_maior].prioridade) { /* pausa o processo id_maior */
                sem_wait(&sem_nucleo);
                if(debbug) fprintf(stderr, "Processo \"%s\" foi pausado, liberando a CPU %d\n\tPara troca de contexto\n", proc[id_maior].nome, cpu_troca_contexto+1);
                proc[id_maior].estado = 1;
                CPUs[cpu_troca_contexto] = lista->id;
                lista->CPU = cpu_troca_contexto;

                aux = fila = &proc[id_maior];
                fila->prox_fila = lista->prox_fila;
                lista->prox_fila = NULL;
                
                sem_post(&mutex[lista->id]);
                sem_post(&sem_nucleo);
                numero_de_mudancas++;
            }
            lista = lista->next;
        }
        fila = verifica_nucleoLivre_e_processosNaFila(fila);
        tempoFinal = clock();
        tempoGasto = (double)(tempoFinal-tempoInicial)/(double)CLOCKS_PER_SEC;  
    }
    for (lista = init; lista; lista = lista->next) pthread_join(lista->thread_id, NULL);
    return EXIT_SUCCESS;
}

/*-----------------------------------------------*/
/*                                               */
/*   ROUND ROBIN (sem prioridades)               */
/*                                               */
/*                                               */
/*-----------------------------------------------*/
int rr() {
    int i = 0;
    int id_maior;
    int cpu_troca_contexto;
    clock_t inicioQuantum, quantum;

    struct processo *lista = init;
    struct processo *fila  = NULL;
    struct processo *aux   = NULL;

    processos_em_espera = 0;
    numero_de_mudancas  = 0;
    processo_terminou   = 0;
    tempoGasto          = 0;

    nucleos_livres      = 1;

    inicioQuantum = clock();
    tempoInicial = clock();
    while(processo_terminou < numero_de_processos) {
        while(lista && tempoGasto >= lista->t0) {
            criaProcessoRR(lista);

            if(processos_em_espera == 1) fila = lista;
            else aux->prox_fila = lista;
            aux = lista;
            lista = lista->next;

            if(nucleos_livres > 0){
                nucleos_livres--; 
                CPUs[0] = fila->id; 
                fila->CPU = 0; 

            }
        }

        while(processos_em_espera > 1 && quantum > TEMPO_QUANTUM) {
            sem_wait(&sem_nucleo);
                if(fila->estado == 3) {
                    processos_em_espera--;
                    fila = fila->prox_fila;
                }
                else { /* pausa o atual e roda o proximo */
                    nucleos_livres--; /* Sera usado uma CPU */

                    i = 0;                       /*   Nesta parte o escalonador procura o primeiro    */
                    while(CPUs[i] != -1) i++;    /* CPU livre, e com certeza havera uma, essa tbm nao */
                    CPUs[i] = fila->id;          /* vai utilizar um nucleo que nao existe, pois ja    */
                    fila->CPU = i;               /* foi verificado isso anteriormente                 */

                    sem_post(&mutex[fila->id]);
                    fila = fila->prox_fila; /* Pega agora, para numa proxima rodada dessa funcao, o proximo da fila */
                }
            sem_post(&sem_nucleo);
        }

        tempoFinal = clock();
        tempoGasto = (double) (tempoFinal-tempoInicial)/(double)CLOCKS_PER_SEC; 
        quantum    = (double)(tempoFinal-inicioQuantum)/(double)CLOCKS_PER_SEC; 
    }
    for (lista = init; lista; lista = lista->next) pthread_join(lista->thread_id, NULL);
    return EXIT_SUCCESS;
}

/*_________________________________________________________________________________________________________
/*
/*                                                                                                    MAIN 
/*_________________________________________________________________________________________________________
*/
int main (int argc, char *argv[]) {
    int mode;
    int   i = 0;
    char  line[150];

    /* Guardar variaveis do arquivo trace */
    float t0, dt, dl;
    char  nome[100]; 
    int   p;

    numero_de_processos = 0;
    numero_de_mudancas  = 0;
    processo_terminou   = 0;

    /* Verifica a quantidade de nucleos Online */
    nucleos_livres = sysconf(_SC_NPROCESSORS_ONLN);
    if(nucleos_livres < 1) nucleos_livres = 1;

    mode   = atoi(argv[1]);
    if(mode < 1 || mode > 6) mode = 1;
    trace  = fopen(argv[2], "r");
    output = fopen(argv[3], "w");

    if(argc > 4 && strcmp(argv[4], "d") == 0 ) debbug = 1;
    else debbug = 0;

    if(!output || !trace) {
        printf("Erro, nao foi possivel abrir o arquivo\n");
        return EXIT_FAILURE;
    }

    /* Inicializa todas as CPU, com nenhum processo sendo executado */
    for(i = 0; i < 32; i++) CPUs[i] = -1;

    /* Inicializa o semáforo */
    for(i = 0; i < 128; i++){ 
        if (sem_init(&mutex[i],0,0)) {
            printf("Erro ao criar o semáforo!\n");
            return EXIT_FAILURE;
        }
    }
    if (sem_init(&sem_nucleo,0,1)) {
        printf("Erro ao criar o semáforo!\n");
        return EXIT_FAILURE;
    }
    
    proc = calloc(128, sizeof(PROCESSO));
    if (!proc) {
        printf("Não foi possível alocar memória!\n");
        return EXIT_FAILURE;
    }

    while(!feof(trace)) {
        i = 0;
        fgets(line,150,trace);
        sscanf(line, "%f %s %f %f %d", &t0, nome, &dt, &dl, &p);
        proc[numero_de_processos].id         = numero_de_processos;
        proc[numero_de_processos].estado     = 1;
        proc[numero_de_processos].t0         = t0;
        proc[numero_de_processos].dt         = dt;
        proc[numero_de_processos].remain     = dt;
        proc[numero_de_processos].deadline   = dl;
        proc[numero_de_processos].prioridade = p;
        proc[numero_de_processos].CPU        = -1;
        proc[numero_de_processos].nome[i]    = 0;
        proc[numero_de_processos].next       = NULL;
        proc[numero_de_processos].prox_fila  = NULL;
        while(nome[i] != 0) {
            proc[numero_de_processos].nome[i] = nome[i];
            i++;
        }
        if(init == NULL) {
            init  = &proc[numero_de_processos];
            final = &proc[numero_de_processos];
        }
        else {
            final->next = &proc[numero_de_processos];
            final       = final->next;
        }
        numero_de_processos++;
    }

    switch(mode) {
    case 1 : fcfs(); break;
    case 2 : sjf();  break;
    case 3 : srtn(); break;
    case 4 :
    break;
    case 5: escalonamentoPrioridade(); break;
    case 6:
    break;
    }
    fprintf(output, "%d", numero_de_mudancas);
    if(debbug) fprintf(stderr, "Ao total foi/foram %d mudanca(s) de contexto\n", numero_de_mudancas);

    free(proc);
    fclose(output);
    fclose(trace);

    return EXIT_SUCCESS;
}