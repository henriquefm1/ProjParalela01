#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

//estrutura para armazenar o arquivo de forma mais compacta
typedef struct{
    char sensor_id[32];
    char tipo[20];
    double valor;
    char status[10];
} Leitura;


//mesma estrutura que a sua
typedef struct {
    char   nome[32];
    double soma_temp;
    double soma_quad_temp;
    long   count_temp;
    double soma_umid;
    double soma_quad_umid;
    long   count_umid;
    double soma_energia;
    long   count_energia;
    long   alertas;
    long   criticos;
} Status_sensor;

//estrutura para passar os limites do vetor para a thread
typedef struct {
    int id_thread;
    long inicio;
    long fim;
} Argumentos_Thread;

//variaveis globais
//vetor com TODAS as linhas do arquivo
pthread_mutex_t mutex_global;
Leitura *dados_globais = NULL;

//totais globais
long total_alertas_globais = 0;
long total_criticos_globais = 0;
double total_energia_global = 0.0;

//sensores globais
Status_sensor *sensores_globais = NULL;
int n_sensores_globais = 0;
int cap_sensores = 0;

//funções auxiliares
Status_sensor *buscar_criar_global(const char *nome){

}

double desvio_padrao(double soma, double soma_q, long n){

}

int comparador_nome(const void *a, const void *b){

}

//função das threads
void *processar_bloco(void *arg){
    
}

int main(int argc, char *argv[]){
    if (argc < 3){
        fprintf(stderr, "Uso: %s <num_threads> <arquivo_log>\n", argv[0]);
            return 1;
    }

    int num_threads = atoi(argv[1]);
    char *nome_arquivo = argv[2];

    //vetor dinamico
    long capacidade = 1000000; //começa com 1 milhao
    long total_leituras = 0;

    Leitura *dados = malloc(capacidade * sizeof(Leitura));

    if (!dados){
        fprinf(stderr, "Erro na memoria\n");
        return 1;
    }

    //abrir arquivo
    FILE *f = fopen(nome_arquivo, "r");
    if (!f){
        perror("Erro ao abrir arquivo");
        return 1;
    }

    char linha[256];
    char data[12];
    char hora[10];
    char lixo[10];

    //ler linha por linha e salvwr no vetor
    while(fgets(linha, sizeof(linha), f)){
        if(total_leituras >= capacidade){
            capacidade = capacidade * 2;
            dados = realloc(dados, capacidade * sizeof(Leitura));
        }

        //estrair dados da linha para a struct
        int dados_lidos = sscanf(linha, "%s %s %s %s %lf %s %s", dados[total_leituras].sensor_id, data, hora, 
            dados[total_leituras].tipo, &dados[total_leituras].valor, lixo, dados[total_leituras].status);

            //se le as 7 palavras corretamente, valida a leitura
            if(dados_lidos == 7){
                total_leituras++;
            }
    }
    fclose(f);

    //achei melhor fazer um array dinâmico de structs e descartei a data/hora durante o parsing inicial.
    //reduziu o consumo de memória RAM e garantiu que as threads tivessem acesso
    //rapido por meio de indices contiguos na memoria

    printf("Total de linhas: %ld\n", total_leituras);


    free(dados);
    return 0;
}