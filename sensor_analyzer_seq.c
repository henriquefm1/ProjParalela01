//Henrique Ferreira Marciano RA: 10439797
//Pedro Casas Pequeno Junior RA: 10437031
//Pedro Henrique Saraiva Arruda RA: 10437747

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>


// estrutura de estatísticas por sensor
// não armazenamos as linhas em memória, atualizamos estatísticas incrementalmente (soma, soma de quadrados, contagem)

typedef struct {
    char   nome[32];
    // temperatura
    double soma_temp;
    double soma_quad_temp;
    long   count_temp;
    // umidade 
    double soma_umid;
    double soma_quad_umid;
    long   count_umid;
    // energia 
    double soma_energia;
    long   count_energia;
    // alertas 
    long   alertas;
    long   criticos;
} Status_sensor;


static Status_sensor *sensores   = NULL;
static int  n_sensores = 0;
static int  cap = 0;

static Status_sensor *buscar_ou_criar(const char *nome) {
    for (int i = 0; i < n_sensores; i++)
        if (strcmp(sensores[i].nome, nome) == 0)
            return &sensores[i];

    if (n_sensores == cap) {
        cap = (cap == 0) ? 128 : cap * 2;
        Status_sensor *tmp = realloc(sensores, cap * sizeof(Status_sensor));
        if (!tmp) { fprintf(stderr, "Sem memoria!\n"); exit(1); }
        sensores = tmp;
    }

    Status_sensor *s = &sensores[n_sensores++];
    memset(s, 0, sizeof(*s));
    snprintf(s->nome, sizeof(s->nome), "%s", nome);
    return s;
}

// formato: sensor_id data hora tipo valor status status

static void processar_linha(char *linha) {
    char sensor_id[32], tipo[20], status[10];
    char data[12], hora[10], lixo[10];
    double valor;

    // lê todos os campos de uma vez
    int lidos = sscanf(linha, "%s %s %s %s %lf %s %s", sensor_id, data, hora, tipo, &valor, lixo, status);
    if (lidos != 7) return;  // linha incompleta ou vazia, ignora

    Status_sensor *s = buscar_ou_criar(sensor_id);

    if (strcmp(tipo, "temperatura") == 0) {
        s->soma_temp      += valor;
        s->soma_quad_temp += valor * valor;
        s->count_temp++;
    } else if (strcmp(tipo, "umidade") == 0) {
        s->soma_umid      += valor;
        s->soma_quad_umid += valor * valor;
        s->count_umid++;
    } else if (strcmp(tipo, "energia") == 0) {
        s->soma_energia += valor;
        s->count_energia++;
    }

    if (strcmp(status, "ALERTA")  == 0) s->alertas++;
    if (strcmp(status, "CRITICO") == 0) { s->alertas++; s->criticos++; }
}


static double dp(double soma, double soma_q, long n) {
    if (n < 2) return 0.0;

    double media    = soma / n;
    double media_q  = soma_q / n;       // média dos quadrados
    double varianca = media_q - media * media;  // E[x²] - E[x]²

    if (varianca < 0.0) varianca = 0.0; // proteção contra erro de ponto flutuante
    return sqrt(varianca);
}

static int cmp_nome(const void *a, const void *b) {
    Status_sensor *sa = (Status_sensor*)a;
    Status_sensor *sb = (Status_sensor*)b;
    return strcmp(sa->nome, sb->nome);
}


int main(int argc, char *argv[]) {
    if (argc < 2) { fprintf(stderr, "Uso: %s <arquivo_log>\n", argv[0]); return 1; }

    FILE *f = fopen(argv[1], "r");
    if (!f) { perror("fopen"); return 1; }

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    char linha[256];
    long total_linhas = 0;
    while (fgets(linha, sizeof(linha), f)) {
        total_linhas++;
        processar_linha(linha);
    }
    fclose(f);

    clock_gettime(CLOCK_MONOTONIC, &t1);
    double elapsed = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) * 1e-9;

    /* Totais globais */
    long   total_alertas  = 0;
    long   total_criticos = 0;
    double total_energia  = 0.0;
    for (int i = 0; i < n_sensores; i++) {
        total_alertas  += sensores[i].alertas;
        total_criticos += sensores[i].criticos;
        total_energia  += sensores[i].soma_energia;
    }

    qsort(sensores, n_sensores, sizeof(Status_sensor), cmp_nome);

    printf("── Media de temperatura (10 primeiros sensores) ────────\n");
    printf("  %-15s  %10s  %11s  %9s\n", "Sensor", "Media(°C)", "Desvio(°C)", "Leituras");


    int exibidos = 0;
    for (int i = 0; i < n_sensores && exibidos < 10; i++) {
        Status_sensor *s = &sensores[i];
        if (s->count_temp == 0) continue;
        printf("  %-15s  %10.2f  %11.4f  %9ld\n",
               s->nome,
               s->soma_temp / s->count_temp,
               dp(s->soma_temp, s->soma_quad_temp, s->count_temp),
               s->count_temp);
        exibidos++;
    }

    // sensor mais instavel
    Status_sensor *instavel = NULL;
    double maior_dp = -1.0;
    for (int i = 0; i < n_sensores; i++) {
        if (sensores[i].count_temp < 2) continue;
        double d = dp(sensores[i].soma_temp, sensores[i].soma_quad_temp, sensores[i].count_temp);
        if (d > maior_dp) { maior_dp = d; instavel = &sensores[i]; }
    }

    printf("\n── Sensor mais instavel ────────────────────────────────\n");
    if (instavel)
        printf("  Sensor : %s\n  Desvio : %.4f °C\n  Leituras: %ld\n", instavel->nome, maior_dp, instavel->count_temp);
    else
        printf("  Nenhum sensor de temperatura encontrado.\n");

    printf("\n── Totais ──────────────────────────────────────────────\n");
    printf("  Total de alertas          : %ld\n",  total_alertas);
    printf("  Somente CRITICO           : %ld\n",  total_criticos);
    printf("  Consumo total de energia  : %.2f W\n", total_energia);

    printf("\n── Desempenho ──────────────────────────────────────────\n");
    printf("  Linhas processadas : %ld\n",   total_linhas);
    printf("  Sensores distintos : %d\n",    n_sensores);
    printf("  Tempo de execucao  : %.3f s\n", elapsed);

    free(sensores);
    return 0;
}
