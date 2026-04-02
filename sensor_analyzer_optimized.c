//Henrique Ferreira Marciano RA: 10439797
//Pedro Casas Pequeno Junior RA: 10437031
//Pedro Henrique Saraiva Arruda RA: 10437747

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>

//estruturas
typedef struct{
    char sensor_id[32];
    char tipo[20];
    double valor;
    char status[10];
} Leitura;

//armazena as estatisticas calculadas de cada sensor
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

//agora cada thread tem suas próprias variáveis locais
typedef struct {
    int id_thread;
    long inicio;
    long fim;
    
    //acumuladores locais
    long total_alertas_locais;
    long total_criticos_locais;
    double total_energia_local;
    
    //lista de sensores particular da thread
    Status_sensor *sensores_locais;
    int n_sensores_locais;
    int cap_sensores_locais;
} Argumentos_Thread;

//variaveis globais
Leitura *dados_globais = NULL;

//totais finais globais
long total_alertas_globais = 0;
long total_criticos_globais = 0;
double total_energia_global = 0.0;

//lista de sensores final (onde vamos juntar tudo)
Status_sensor *sensores_globais = NULL;
int n_sensores_globais = 0;
int cap_sensores = 0;

//funções auxiliares
Status_sensor *buscar_criar_global(const char *nome){
    //tenta encontrar o sensor existente
    for (int i = 0; i < n_sensores_globais; i++){
        if(strcmp(sensores_globais[i].nome, nome) == 0){
            return &sensores_globais[i];
        }
    }
    //se não encontra e o vetor esta cheio, dobra a capacidade
    if (n_sensores_globais == cap_sensores){
        if (cap_sensores == 0){
            cap_sensores = 128;
        }else{
            cap_sensores = cap_sensores * 2;
        }
        Status_sensor *tmp = realloc(sensores_globais, cap_sensores * sizeof(Status_sensor));
        if (!tmp) { exit(1); }
        sensores_globais = tmp;
    }
    //imicializa novo sensor
    int posicao_livre = n_sensores_globais;
    Status_sensor *novo_sensor = &sensores_globais[posicao_livre];
    memset(novo_sensor, 0, sizeof(Status_sensor));
    strncpy(novo_sensor->nome, nome, sizeof(novo_sensor->nome) - 1);
    novo_sensor->nome[sizeof(novo_sensor->nome) - 1] = '\0';
    n_sensores_globais++;
    return novo_sensor;
}

//função para buscar ou criar um sensor na lista local da thread.
//como cada thread chama isso apenas para o betor
Status_sensor *buscar_criar_local(Argumentos_Thread *args, const char *nome){
    //busca na lista exclusiva da thread atual
    for (int i = 0; i < args->n_sensores_locais; i++){
        if(strcmp(args->sensores_locais[i].nome, nome) == 0){
            return &args->sensores_locais[i];
        }
    }
    //expansão dinâmica do vetor particular da thread
    if (args->n_sensores_locais == args->cap_sensores_locais){
        if (args->cap_sensores_locais == 0){
            args->cap_sensores_locais = 128;
        } else {
            args->cap_sensores_locais *= 2;
        }
        Status_sensor *tmp = realloc(args->sensores_locais, args->cap_sensores_locais * sizeof(Status_sensor));
        if (!tmp) { exit(1); }
        args->sensores_locais = tmp;
    }
    //configura o novo sensor localmente
    int posicao_livre = args->n_sensores_locais;
    Status_sensor *novo_sensor = &args->sensores_locais[posicao_livre];
    memset(novo_sensor, 0, sizeof(Status_sensor));
    strncpy(novo_sensor->nome, nome, sizeof(novo_sensor->nome) - 1);
    novo_sensor->nome[sizeof(novo_sensor->nome) - 1] = '\0';
    args->n_sensores_locais++;
    return novo_sensor;
}

//calcula o desvio padrão usando a fórmula da variância: E[X^2] - (E[X])^2
double desvio_padrao(double soma, double soma_q, long n){
    if(n < 2) return 0.0;
    double media = soma / n;
    double varianca = (soma_q / n) - (media * media);
    if(varianca < 0.0) varianca = 0.0; // Evita raiz quadrada de número negativo por erro de ponto flutuante
    return sqrt(varianca);
}

//comparador para ordenar os sensores por nome alfabeticamente no final
int comparador_nome(const void *a, const void *b){
    Status_sensor *sa = (Status_sensor*)a;
    Status_sensor *sb = (Status_sensor*)b;
    return strcmp(sa->nome, sb->nome);
}

//função das threads otimizadas
void *processar_bloco(void *arg){
    Argumentos_Thread *args = (Argumentos_Thread*) arg;

    //Inicializa o espaço de trabalho local da thread
    args->total_alertas_locais = 0;
    args->total_criticos_locais = 0;
    args->total_energia_local = 0.0;
    args->sensores_locais = NULL;
    args->n_sensores_locais = 0;
    args->cap_sensores_locais = 0;

    //processamento livre (Lock-free). As threads não precisam esperar em fila
    for(long i = args->inicio; i < args->fim; i++){
        Leitura atual = dados_globais[i];

        //busca o sensor no vetor exclusivo dessa thread
        Status_sensor *s = buscar_criar_local(args, atual.sensor_id);

        //atualiza as somas na struct local do sensor
        if(strcmp(atual.tipo, "temperatura") == 0){
            s->soma_temp += atual.valor;
            s->soma_quad_temp += atual.valor * atual.valor;
            s->count_temp++;
        }else if(strcmp(atual.tipo, "umidade") == 0){
            s->soma_umid += atual.valor;
            s->soma_quad_umid += atual.valor * atual.valor;
            s->count_umid++;
        }else if(strcmp(atual.tipo, "energia") == 0){
            s->soma_energia += atual.valor;
            s->count_energia++;
            args->total_energia_local += atual.valor; //atualiza total lcoal
        }

        //atualiza os contadores de alerta local
        if(strcmp(atual.status, "ALERTA") == 0){
            s->alertas++;
            args->total_alertas_locais++;
        }else if(strcmp(atual.status, "CRITICO") == 0){
            s->alertas++;
            s->criticos++;
            args->total_alertas_locais++;
            args->total_criticos_locais++;
        }
    }
    //thread termina e deixa os resultados salvos na struct (`)args) que a main envia
    pthread_exit(NULL);
}

//main
int main(int argc, char *argv[]){
    if (argc < 3){
        fprintf(stderr, "Uso: %s <num_threads> <arquivo_log>\n", argv[0]);
        return 1;
    }

    int num_threads = atoi(argv[1]);
    char *nome_arquivo = argv[2];

    //carrega dados
    long capacidade = 1000000;
    long total_leituras = 0;
    dados_globais = malloc(capacidade * sizeof(Leitura));

    FILE *f = fopen(nome_arquivo, "r");
    char linha[256], data[12], hora[10], lixo[10];

    //joga na ram lendo linha por linha
    while(fgets(linha, sizeof(linha), f)){
        //caso base, se enche o vetor, dobra o tamanho
        if(total_leituras >= capacidade){
            capacidade *= 2;
            dados_globais = realloc(dados_globais, capacidade * sizeof(Leitura));
        }
        //separa as colunas em variáveis, descartando data e hora (otimização de RAM)
        int dados_lidos = sscanf(linha, "%s %s %s %s %lf %s %s", 
            dados_globais[total_leituras].sensor_id, data, hora, 
            dados_globais[total_leituras].tipo, &dados_globais[total_leituras].valor, 
            lixo, dados_globais[total_leituras].status);

        if(dados_lidos == 7) total_leituras++;
    }
    fclose(f);

    //cronometro
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    //divide trabalho e cria threads
    pthread_t threads[num_threads];
    Argumentos_Thread args[num_threads];
    long tamanho_bloco = total_leituras / num_threads;

    for(int i = 0; i < num_threads; i++){
        args[i].id_thread = i;
        args[i].inicio = i * tamanho_bloco;
        //a ultima thrwead pega o resto da divisao para nao perder nenhuma linha
        if(i == num_threads - 1) args[i].fim = total_leituras;
        else args[i].fim = (i + 1) * tamanho_bloco;

        //cria thread chamando processar_bloco
        pthread_create(&threads[i], NULL, processar_bloco, (void*)&args[i]);
    }

    for(int i = 0; i < num_threads; i++){
        pthread_join(threads[i], NULL);
    }

    //fusão dos resultados
    for(int i = 0; i < num_threads; i++){
        //soma totais diretos
        total_energia_global += args[i].total_energia_local;
        total_alertas_globais += args[i].total_alertas_locais;
        total_criticos_globais += args[i].total_criticos_locais;

        //percorre os sensores que a thread processooiu
        for(int j = 0; j < args[i].n_sensores_locais; j++){
            Status_sensor *s_local = &args[i].sensores_locais[j];
            //buscar criar glocal
            Status_sensor *s_global = buscar_criar_global(s_local->nome);

            //soma tudo o que a thread fez na caixa global definitiva
            s_global->soma_temp += s_local->soma_temp;
            s_global->soma_quad_temp += s_local->soma_quad_temp;
            s_global->count_temp += s_local->count_temp;
            
            s_global->soma_umid += s_local->soma_umid;
            s_global->soma_quad_umid += s_local->soma_quad_umid;
            s_global->count_umid += s_local->count_umid;
            
            s_global->soma_energia += s_local->soma_energia;
            s_global->count_energia += s_local->count_energia;
            
            s_global->alertas += s_local->alertas;
            s_global->criticos += s_local->criticos;
        }
        //libera memoria que a thread alocou pro vetor
        free(args[i].sensores_locais);
    }

    //para o cronometro
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double elapsed = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) * 1e-9;

    //ordena o array global com qsort
    qsort(sensores_globais, n_sensores_globais, sizeof(Status_sensor), comparador_nome);

    //resultados
    printf("── Media de temperatura (10 primeiros sensores) ────────\n");
    printf("  %-15s  %10s  %11s  %9s\n", "Sensor", "Media(°C)", "Desvio(°C)", "Leituras");

    int exibidos = 0;
    //imprime no maximo os 10 primeirtos
    for(int i = 0; i < n_sensores_globais && exibidos < 10; i++){
        Status_sensor *s = &sensores_globais[i];
        if(s->count_temp == 0) continue;
        printf("  %-15s  %10.2f  %11.4f  %9ld\n", 
               s->nome, s->soma_temp / s->count_temp, 
               desvio_padrao(s->soma_temp, s->soma_quad_temp, s->count_temp), 
               s->count_temp);
        exibidos++;
    }

    //busca o sensor com maior desvio padrao de temperatura
    Status_sensor *instavel = NULL;
    double maior_dp = -1.0;
    for(int i = 0; i < n_sensores_globais; i++) {
        if (sensores_globais[i].count_temp < 2) continue;
        double d = desvio_padrao(sensores_globais[i].soma_temp, sensores_globais[i].soma_quad_temp, sensores_globais[i].count_temp);
        if(d > maior_dp){
             maior_dp = d; 
             instavel = &sensores_globais[i]; 
        }
    }

    printf("\n── Sensor mais instavel ────────────────────────────────\n");
    if(instavel)
        printf("  Sensor : %s\n  Desvio : %.4f °C\n  Leituras: %ld\n", instavel->nome, maior_dp, instavel->count_temp);
    else
        printf("  Nenhum sensor de temperatura encontrado.\n");

    printf("\n── Totais ──────────────────────────────────────────────\n");
    printf("  Total de alertas          : %ld\n",  total_alertas_globais);
    printf("  Somente CRITICO           : %ld\n",  total_criticos_globais);
    printf("  Consumo total de energia  : %.2f W\n", total_energia_global);

    printf("\n── OTIMIZADO: Desempenho ───────────────────────────────\n");
    printf("  Threads utilizadas : %d\n",    num_threads);
    printf("  Linhas processadas : %ld\n",   total_leituras);
    printf("  Sensores distintos : %d\n",    n_sensores_globais);
    printf("  Tempo de execucao  : %.3f s\n", elapsed);

    free(dados_globais);
    free(sensores_globais);
    return 0;
}
