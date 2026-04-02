//Henrique Ferreira Marciano RA: 10439797
//Pedro Casas Pequeno Junior RA: 10437031
//Pedro Henrique Saraiva Arruda RA: 10437747

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h> // adicionei a math.h por causa do sqrt()
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
    for (int i = 0; i < n_sensores_globais; i++){
        if(strcmp(sensores_globais[i].nome, nome) == 0){
            return &sensores_globais[i];
        }
    }
    
    if (n_sensores_globais == cap_sensores){
        if (cap_sensores == 0){
            cap_sensores = 128;
        }else{
            cap_sensores = cap_sensores * 2;
        }
        Status_sensor *tmp = realloc(sensores_globais, cap_sensores * sizeof(Status_sensor));

        if (!tmp){
            fprintf(stderr, "Sem memoria para os sensores\n");
            exit(1);
        }
        sensores_globais = tmp;
    }

    //pegar a posição atual vazia no vetor
    int posicao_livre = n_sensores_globais;
    Status_sensor *novo_sensor = &sensores_globais[posicao_livre];
    //zerar todas as somas que começam com 0
    memset(novo_sensor, 0, sizeof(Status_sensor));
    //copia o novo nome do sensor
    strncpy(novo_sensor->nome, nome, sizeof(novo_sensor->nome) - 1);
    //garante que a string termina sem erros
    novo_sensor->nome[sizeof(novo_sensor->nome) - 1] = '\0';
    //adiciona mais um sensor nos sensores globais
    n_sensores_globais = n_sensores_globais + 1;
    //retorna o sensor
    return novo_sensor;
}

//calcula desvio padrao
double desvio_padrao(double soma, double soma_q, long n){
    if(n < 2){
        return 0.0;
    }
    double media = soma / n;
    double media_q = soma_q / n;
    double varianca = media_q - media * media;
    if(varianca < 0.0){
        varianca = 0.0;
    }
    return sqrt(varianca);
}

//compara os sensores por nome para ordena-los no final
int comparador_nome(const void *a, const void *b){
    Status_sensor *sa = (Status_sensor*)a;
    Status_sensor *sb = (Status_sensor*)b;
    return strcmp(sa->nome, sb->nome);
}

//função das threads
void *processar_bloco(void *arg){
    Argumentos_Thread *args = (Argumentos_Thread*) arg;

    for(long i = args->inicio; i < args->fim; i++){
        Leitura atual = dados_globais[i];

        //começo da seção critica
        pthread_mutex_lock(&mutex_global);
        
        Status_sensor *s = buscar_criar_global(atual.sensor_id);

        //atualiza os dados especificos do sensor
        if(strcmp(atual.tipo, "temperatura") == 0){
            s->soma_temp += atual.valor;
            s->soma_quad_temp += atual.valor * atual.valor;
            s->count_temp++;
        }else if(strcmp(atual.tipo, "umidade") == 0){
            s->soma_umid += atual.valor;
            s->soma_quad_umid += atual.valor * atual.valor; // corrigido para quadrado
            s->count_umid++;
        }else if(strcmp(atual.tipo, "energia") == 0){
            s->soma_energia += atual.valor;
            s->count_energia++;
            total_energia_global += atual.valor;
        }

        //atualiza os alertas
        if(strcmp(atual.status, "ALERTA") == 0){
            s->alertas++;
            total_alertas_globais++;
        }else if(strcmp(atual.status, "CRITICO") == 0){
            s->alertas++;
            s->criticos++;
            total_alertas_globais++;
            total_criticos_globais++;
        }
        
        pthread_mutex_unlock(&mutex_global);
        //fim da seção critica
    }
    pthread_exit(NULL);
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

    dados_globais = malloc(capacidade * sizeof(Leitura));

    if (!dados_globais){
        fprintf(stderr, "Erro na memoria\n");
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

    //ler linha por linha e salvar no vetor
    while(fgets(linha, sizeof(linha), f)){
        if(total_leituras >= capacidade){
            capacidade = capacidade * 2;
            dados_globais = realloc(dados_globais, capacidade * sizeof(Leitura));
        }

        //extrair dados da linha para a struct
        int dados_lidos = sscanf(linha, "%s %s %s %s %lf %s %s", dados_globais[total_leituras].sensor_id, data, hora, 
            dados_globais[total_leituras].tipo, &dados_globais[total_leituras].valor, lixo, dados_globais[total_leituras].status);

            //se le as 7 palavras corretamente, valida a leitura
            if(dados_lidos == 7){
                total_leituras++;
            }
    }
    fclose(f);

    printf("Total de linhas: %ld\n", total_leituras);

    //iniciar cronometros e thread
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    //inicia mutex
    pthread_mutex_init(&mutex_global, NULL);

    pthread_t threads[num_threads];
    Argumentos_Thread args[num_threads];

    long tamanho_bloco = total_leituras / num_threads;

    //criar threads e divisão do trabalho
    for(int i = 0; i < num_threads; i++){
        args[i].id_thread = i;
        args[i].inicio = i * tamanho_bloco;

        if(i == num_threads - 1){
            args[i].fim = total_leituras;
        } else{
            args[i].fim = (i + 1) * tamanho_bloco;
        }

        pthread_create(&threads[i], NULL, processar_bloco, (void*)&args[i]);
    }

    for(int i = 0; i < num_threads; i++){
        pthread_join(threads[i], NULL);
    }

    pthread_mutex_destroy(&mutex_global);

    //para o cronometro
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double elapsed = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) * 1e-9;

    //resultados
    qsort(sensores_globais, n_sensores_globais, sizeof(Status_sensor), comparador_nome);

    printf("── Media de temperatura (10 primeiros sensores) ────────\n");
    printf("  %-15s  %10s  %11s  %9s\n", "Sensor", "Media(°C)", "Desvio(°C)", "Leituras");

    int exibidos = 0;
    for(int i = 0; i < n_sensores_globais && exibidos < 10; i++){
        Status_sensor *s = &sensores_globais[i];
        if(s->count_temp == 0){
            continue;
        }
        printf("  %-15s  %10.2f  %11.4f  %9ld\n", s->nome, s->soma_temp / s->count_temp, desvio_padrao(s->soma_temp, s->soma_quad_temp, s->count_temp), s->count_temp);
        exibidos++;
    }

    Status_sensor *instavel = NULL;
    double maior_dp = -1.0;
    for(int i = 0; i < n_sensores_globais; i++) {
        if (sensores_globais[i].count_temp < 2){
            continue;
        }
        double d = desvio_padrao(sensores_globais[i].soma_temp, sensores_globais[i].soma_quad_temp, sensores_globais[i].count_temp);
        if(d > maior_dp){
             maior_dp = d; instavel = &sensores_globais[i]; 
        }
    }

    printf("\n── Sensor mais instavel ────────────────────────────────\n");
    if(instavel){
        printf("  Sensor : %s\n  Desvio : %.4f °C\n  Leituras: %ld\n", instavel->nome, maior_dp, instavel->count_temp);
    }else{
        printf("  Nenhum sensor de temperatura encontrado.\n");
    }

    printf("\n── Totais ──────────────────────────────────────────────\n");
    printf("  Total de alertas          : %ld\n",  total_alertas_globais);
    printf("  Somente CRITICO           : %ld\n",  total_criticos_globais);
    printf("  Consumo total de energia  : %.2f W\n", total_energia_global);

    printf("\n── Desempenho ──────────────────────────────────────────\n");
    printf("  Threads utilizadas : %d\n",    num_threads);
    printf("  Linhas processadas : %ld\n",   total_leituras);
    printf("  Sensores distintos : %d\n",    n_sensores_globais);
    printf("  Tempo de execucao  : %.3f s\n", elapsed);

    //libera a memoria
    free(dados_globais);
    free(sensores_globais);

    return 0;
}
