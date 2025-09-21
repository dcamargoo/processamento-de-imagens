#include <stdio.h>
#include <stdint.h> // para usar o uint8_t
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

// Retorna 1 se estiver em escala de cinza / 0 se não
int verifica_se_imagem_e_cinza(SDL_Surface* img) {

    if (!img) return -1;
    // Garanta que já está em RGBA32 ANTES de chamar esta função
    if (img->format != SDL_PIXELFORMAT_RGBA32) return -1;

    int w = img->w, h = img->h;
    int pitch = img->pitch;
    unsigned char* base = (unsigned char*)img->pixels;

    for (int y = 0; y < h; y++) {
        unsigned char* row = base + y * pitch;
        for (int x = 0; x < w; x++) {
            unsigned char* px = row + x * 4; 
            int R = px[0], G = px[1], B = px[2];
            if (!(R == G && G == B)) {
                return 0;
            }
        }
    }

    return 1; // está em escala de cinza
}

int aplicar_escala_de_cinza(SDL_Surface* img) {
    if (!img) return -1;
    if (img->format != SDL_PIXELFORMAT_RGBA32) return -1;

    int w = img->w, h = img->h;
    int pitch = img->pitch;
    uint8_t* base = (uint8_t*)img->pixels;

    for (int y = 0; y < h; y++) {
        uint8_t* row = base + y * pitch;
        for (int x = 0; x < w; x++) {
            uint8_t* p = row + x * 4; 
            uint8_t R = p[0], G = p[1], B = p[2];
            uint8_t Y = (uint8_t)(0.2125*R + 0.7154*G + 0.0721*B); // fórmula do PDF
            p[0] = Y;  // R
            p[1] = Y;  // G
            p[2] = Y;  // B
        }
    }
    return 0;
}

typedef struct { // valores de intensidade para equalização
    uint8_t antigo; 
    uint8_t novo;   
} ParIntensidade;


typedef struct { // contagem de cada intensidade
    uint8_t intensidade; 
    uint32_t quantidade;
} ContagemIntensidade;

static int comparar_por_intensidade(const void* a, const void* b) {
    const ContagemIntensidade* A = (const ContagemIntensidade*)a;
    const ContagemIntensidade* B = (const ContagemIntensidade*)b;
    return (int)A->intensidade - (int)B->intensidade;
}

// Cria a matriz de mapeamento antigo/novo
size_t criar_matriz_mapeamento_por_imagem(SDL_Surface* imagem, ParIntensidade** saidaPares) {
    if (!imagem || !saidaPares) return 0;
    if (imagem->format != SDL_PIXELFORMAT_RGBA32) { *saidaPares = NULL; return 0; }

    ContagemIntensidade* listaContagens = NULL;
    size_t quantidadeIntensidades = 0;
    uint32_t totalPixels = 0;

    int largura = imagem->w, altura = imagem->h, bytesPorLinha = imagem->pitch;
    uint8_t* inicioPixels = (uint8_t*)imagem->pixels;

    for (int y = 0; y < altura; y++) {
        uint8_t* inicioLinha = inicioPixels + y * bytesPorLinha;
        for (int x = 0; x < largura; x++) {
            uint8_t* pixel = inicioLinha + x * 4;
            uint8_t intensidadeDoPixel = pixel[0];

            size_t i;
            for (i = 0; i < quantidadeIntensidades; i++) {
                if (listaContagens[i].intensidade == intensidadeDoPixel) {
                    listaContagens[i].quantidade++;
                    break;
                }
            }
            if (i == quantidadeIntensidades) {
                ContagemIntensidade* novaLista = (ContagemIntensidade*)
                    realloc(listaContagens, (quantidadeIntensidades + 1) * sizeof(ContagemIntensidade));
                if (!novaLista) { free(listaContagens); *saidaPares = NULL; return 0; }
                listaContagens = novaLista;
                listaContagens[quantidadeIntensidades].intensidade = intensidadeDoPixel;
                listaContagens[quantidadeIntensidades].quantidade  = 1;
                quantidadeIntensidades++;
            }
            totalPixels++;
        }
    }

    if (quantidadeIntensidades == 0) { *saidaPares = NULL; return 0; }

    // Ordena por intensidade para calculo posterior
    qsort(listaContagens, quantidadeIntensidades, sizeof(ContagemIntensidade), comparar_por_intensidade);

    uint32_t somaAteAqui = 0;
    uint32_t primeiraSoma = 0;
    int primeiraSomaDefinida = 0;
    for (size_t i = 0; i < quantidadeIntensidades; i++) {
        somaAteAqui += listaContagens[i].quantidade;
        if (!primeiraSomaDefinida) {
            primeiraSoma = somaAteAqui;
            primeiraSomaDefinida = 1;
        }
    }

    ParIntensidade* pares = (ParIntensidade*)malloc(quantidadeIntensidades * sizeof(ParIntensidade));
    if (!pares) { free(listaContagens); *saidaPares = NULL; return 0; }

    if (totalPixels == 0 || primeiraSoma == totalPixels) {
        for (size_t i = 0; i < quantidadeIntensidades; i++) {
            pares[i].antigo = listaContagens[i].intensidade;
            pares[i].novo   = listaContagens[i].intensidade;
        }
    } else {
        somaAteAqui = 0;
        for (size_t i = 0; i < quantidadeIntensidades; i++) {
            somaAteAqui += listaContagens[i].quantidade;
            double numerador   = (double)((int64_t)somaAteAqui - (int64_t)primeiraSoma);
            double denominador = (double)((int64_t)totalPixels - (int64_t)primeiraSoma);
            long intensidadeEqualizada = lround((numerador / denominador) * 255.0);
            if (intensidadeEqualizada < 0)   intensidadeEqualizada = 0;
            if (intensidadeEqualizada > 255) intensidadeEqualizada = 255;

            pares[i].antigo = listaContagens[i].intensidade;      
            pares[i].novo   = (uint8_t)intensidadeEqualizada;
        }
    }

    free(listaContagens);
    *saidaPares = pares;
    return quantidadeIntensidades;   
}

int equalizar_com_matriz_linear(SDL_Surface* imagemOrigem, SDL_Surface* imagemDestino,
                                const ParIntensidade* paresIntensidade, size_t qtdPares) {
    if (!imagemOrigem || !imagemDestino || (!paresIntensidade && qtdPares>0)) return -1;
    if (imagemOrigem->format != SDL_PIXELFORMAT_RGBA32 || imagemDestino->format != SDL_PIXELFORMAT_RGBA32) return -1;
    if (imagemOrigem->w != imagemDestino->w || imagemOrigem->h != imagemDestino->h) return -1;

    int largura = imagemOrigem->w, altura = imagemOrigem->h;
    int bytesPorLinhaOrigem  = imagemOrigem->pitch;
    int bytesPorLinhaDestino = imagemDestino->pitch;
    uint8_t* pixelsOrigem  = (uint8_t*)imagemOrigem->pixels;
    uint8_t* pixelsDestino = (uint8_t*)imagemDestino->pixels;

    for (int y = 0; y < altura; y++) {
        uint8_t* linhaOrigem  = pixelsOrigem  + y * bytesPorLinhaOrigem;
        uint8_t* linhaDestino = pixelsDestino + y * bytesPorLinhaDestino;
        for (int x = 0; x < largura; x++) {
            uint8_t* pixelOrigem  = linhaOrigem  + x * 4;
            uint8_t* pixelDestino = linhaDestino + x * 4;

            uint8_t intensidadeAntiga = pixelOrigem[0];
            uint8_t intensidadeNova   = intensidadeAntiga;

            for (size_t i = 0; i < qtdPares; i++) {
                if (paresIntensidade[i].antigo == intensidadeAntiga) {
                    intensidadeNova = paresIntensidade[i].novo;
                    break;
                }
            }

            pixelDestino[0] = intensidadeNova; 
            pixelDestino[1] = intensidadeNova; 
            pixelDestino[2] = intensidadeNova; 
            pixelDestino[3] = pixelOrigem[3];
        }
    }
    return 0;
}

int main(int argc, char* argv[]) {

    if (argc < 2) { // verifica se foi recebida a imagem ao executar
        printf("Erro! É preciso passar a imagem ao executar!\n");
        return 1;
    }

    const char* path = argv[1]; // caminho da imagem

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        printf("Erro ao carregar o inicializador do SDL\n");
        return 1;
    }

    (void)IMG_Init(0);

    // Carrega uma Surface para a manipulação dos pixels
    SDL_Surface* initial_img = IMG_Load(path);
    if (!initial_img) {
        printf("Erro ao carregar a imagem na Surface!\n");
        IMG_Quit(); SDL_Quit();
        return 1;
    }

    // Converte para RGBA32 para facilitar o formato
    SDL_Surface* img = SDL_ConvertSurface(initial_img, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(initial_img);
    if (!img) {
        printf("Erro ao converter a imagem do Surface para RGBA32\n");
        IMG_Quit(); SDL_Quit();
        return 1;
    }

    // Lock para acesso aos pixels de forma segura
    if (!SDL_LockSurface(img)) {
        printf("Erro ao fazer o Lock para manipulação!\n");
        SDL_DestroySurface(img); IMG_Quit(); SDL_Quit();
        return 1;
    }

    int escalaCinza = verifica_se_imagem_e_cinza(img);
    if(escalaCinza == 0){
        aplicar_escala_de_cinza(img);
    }

    int w = img->w, h = img->h; // altura e largura dos pixels
    uint8_t* base = (uint8_t*)img->pixels; // usando o tipo de dado uint8_t para dados de 4 Bytes
    int pitch = img->pitch; // bytes por linha

    /* teste
    // Loop pixel a pixel
    for (int y = 0; y < h; y++) {
        uint8_t* row = base + y * pitch;    
        for (int x = 0; x < w; x++) {
            uint8_t* px = row + x * 4;
            uint8_t R = px[0];
            uint8_t G = px[1];
            uint8_t B = px[2];
            uint8_t A = px[3];
        }
    }
    */

    SDL_UnlockSurface(img);

    if (SDL_SaveBMP(img, "saida.bmp")) {
        printf("Processada: %dx%d, salva em 'saida.bmp'\n", w, h);
    } else {
        printf("Erro: falha ao salvar 'saida.bmp'\n");
    }
   
    if (!SDL_LockSurface(img)) {
        printf("Erro ao travar para equalização.\n");
        SDL_DestroySurface(img); IMG_Quit(); SDL_Quit();
        return 1;
    }

    // Cria matriz antigo/novo somente com intensidades presentes (X linhas)
    ParIntensidade* matriz = NULL;
    size_t linhas = criar_matriz_mapeamento_por_imagem(img, &matriz);
    SDL_UnlockSurface(img);

    if (linhas == 0 || !matriz) {
        printf("Erro: não foi possível criar a matriz de mapeamento.\n");
        SDL_DestroySurface(img); IMG_Quit(); SDL_Quit();
        return 1;
    }
    printf("Matriz de mapeamento criada com %zu linhas (colunas = 2: antigo | novo).\n", linhas);

    SDL_Surface* eq = SDL_ConvertSurface(img, SDL_PIXELFORMAT_RGBA32);
    if (!eq) {
        printf("Erro ao criar cópia para equalização.\n");
        free(matriz);
        SDL_DestroySurface(img); IMG_Quit(); SDL_Quit();
        return 1;
    }

    if (!SDL_LockSurface(img) || !SDL_LockSurface(eq)) {
        printf("Erro ao travar surfaces na equalização.\n");
        if (img->locked) SDL_UnlockSurface(img);
        SDL_DestroySurface(eq);
        free(matriz);
        SDL_DestroySurface(img); IMG_Quit(); SDL_Quit();
        return 1;
    }

    // Aplica equalização usando SOMENTE a matriz (sem LUT de 256)
    (void)equalizar_com_matriz_linear(img, eq, matriz, linhas);

    SDL_UnlockSurface(eq);
    SDL_UnlockSurface(img);

    if (SDL_SaveBMP(eq, "saida_eq.bmp")) {
        printf("Equalizada salva em 'saida_eq.bmp'\n");
    } else {
        printf("Erro: falha ao salvar 'saida_eq.bmp'\n");
    }

    free(matriz);
    SDL_DestroySurface(eq);
    SDL_DestroySurface(img);
    IMG_Quit();
    SDL_Quit();
    return 0;
}
