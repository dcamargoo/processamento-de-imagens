// Universidade Presbiteriana Mackenzie – Computação Visual – Proj1 (SDL3)
// Build (Ubuntu):
//   gcc -std=c99 -O2 -Wall -Wextra -o main main.c `pkg-config --cflags --libs sdl3` -lSDL3_image -lm
// Run:
//   ./main caminho/para/imagem.png

#include <stdio.h>
#include <stdint.h>   // uint8_t
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

// === [H] Histograma + estatísticas ===
// Calcula histograma (256 níveis), média e desvio-padrão da imagem em RGBA32 (assumida cinza)
static int calcular_histograma(SDL_Surface* img, uint32_t hist[256], double* media, double* desvio) {
    if (!img || img->format != SDL_PIXELFORMAT_RGBA32) return -1;
    memset(hist, 0, 256 * sizeof(uint32_t));

    int w = img->w, h = img->h, pitch = img->pitch;
    uint8_t* base = (uint8_t*)img->pixels;
    uint64_t soma = 0, soma2 = 0;
    uint64_t N = (uint64_t)w * (uint64_t)h;

    if (!SDL_LockSurface(img)) return -1;
    for (int y = 0; y < h; y++) {
        uint8_t* row = base + y * pitch;
        for (int x = 0; x < w; x++) {
            uint8_t* p = row + x * 4;
            uint8_t v = p[0]; // R==G==B (já cinza)
            hist[v]++;
            soma  += v;
            soma2 += (uint64_t)v * (uint64_t)v;
        }
    }
    SDL_UnlockSurface(img);

    if (N == 0) { if (media) *media = 0.0; if (desvio) *desvio = 0.0; return 0; }
    if (media)  *media  = (double)soma / (double)N;
    if (desvio) {
        double mu = (double)soma / (double)N;
        double var = ((double)soma2 / (double)N) - mu * mu;
        if (var < 0.0) var = 0.0;
        *desvio = sqrt(var);
    }
    return 0;
}

// Retorna 1 se estiver em escala de cinza / 0 se não; -1 para erro/formatos diferentes
int verifica_se_imagem_e_cinza(SDL_Surface* img) {
    if (!img) return -1;
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
    return 1;
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
            uint8_t Y = (uint8_t)(0.2125 * R + 0.7154 * G + 0.0721 * B);
            p[0] = Y; p[1] = Y; p[2] = Y;
        }
    }
    return 0;
}

typedef struct { uint8_t antigo, novo; } ParIntensidade;
typedef struct { uint8_t intensidade; uint32_t quantidade; } ContagemIntensidade;

static int comparar_por_intensidade(const void* a, const void* b) {
    const ContagemIntensidade* A = (const ContagemIntensidade*)a;
    const ContagemIntensidade* B = (const ContagemIntensidade*)b;
    return (int)A->intensidade - (int)B->intensidade;
}

size_t criar_matriz_mapeamento_por_imagem(SDL_Surface* imagem, ParIntensidade** saidaPares) {
    if (!imagem || !saidaPares) return 0;
    if (imagem->format != SDL_PIXELFORMAT_RGBA32) { *saidaPares = NULL; return 0; }

    ContagemIntensidade* listaContagens = NULL;
    size_t quantidadeIntensidades = 0;
    uint32_t totalPixels = 0;

    int w = imagem->w, h = imagem->h, pitch = imagem->pitch;
    uint8_t* base = (uint8_t*)imagem->pixels;

    for (int y = 0; y < h; y++) {
        uint8_t* row = base + y * pitch;
        for (int x = 0; x < w; x++) {
            uint8_t* px = row + x * 4;
            uint8_t intensidade = px[0]; // supomos já cinza

            size_t i;
            for (i = 0; i < quantidadeIntensidades; i++) {
                if (listaContagens[i].intensidade == intensidade) {
                    listaContagens[i].quantidade++;
                    break;
                }
            }
            if (i == quantidadeIntensidades) {
                ContagemIntensidade* nova = realloc(listaContagens, (quantidadeIntensidades + 1) * sizeof(*listaContagens));
                if (!nova) { free(listaContagens); *saidaPares = NULL; return 0; }
                listaContagens = nova;
                listaContagens[quantidadeIntensidades].intensidade = intensidade;
                listaContagens[quantidadeIntensidades].quantidade  = 1;
                quantidadeIntensidades++;
            }
            totalPixels++;
        }
    }

    if (quantidadeIntensidades == 0) { *saidaPares = NULL; return 0; }
    qsort(listaContagens, quantidadeIntensidades, sizeof(ContagemIntensidade), comparar_por_intensidade);

    uint32_t somaAteAqui = 0, primeiraSoma = 0; int primeiraDef = 0;
    for (size_t i = 0; i < quantidadeIntensidades; i++) {
        somaAteAqui += listaContagens[i].quantidade;
        if (!primeiraDef) { primeiraSoma = somaAteAqui; primeiraDef = 1; }
    }

    ParIntensidade* pares = malloc(quantidadeIntensidades * sizeof(*pares));
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
            double num = (double)((int64_t)somaAteAqui - (int64_t)primeiraSoma);
            double den = (double)((int64_t)totalPixels   - (int64_t)primeiraSoma);
            long val = lround((num / den) * 255.0);
            if (val < 0) val = 0;
            if (val > 255) val = 255;
            pares[i].antigo = listaContagens[i].intensidade;
            pares[i].novo   = (uint8_t)val;
        }
    }

    free(listaContagens);
    *saidaPares = pares;
    return quantidadeIntensidades;
}

int equalizar_com_matriz_linear(SDL_Surface* src, SDL_Surface* dst,
                                const ParIntensidade* pares, size_t n) {
    if (!src || !dst || (!pares && n > 0)) return -1;
    if (src->format != SDL_PIXELFORMAT_RGBA32 || dst->format != SDL_PIXELFORMAT_RGBA32) return -1;
    if (src->w != dst->w || src->h != dst->h) return -1;

    int w = src->w, h = src->h;
    int sp = src->pitch, dp = dst->pitch;
    uint8_t* s = (uint8_t*)src->pixels;
    uint8_t* d = (uint8_t*)dst->pixels;

    for (int y = 0; y < h; y++) {
        uint8_t* sr = s + y * sp;
        uint8_t* dr = d + y * dp;
        for (int x = 0; x < w; x++) {
            uint8_t* ps = sr + x * 4;
            uint8_t* pd = dr + x * 4;
            uint8_t oldv = ps[0], newv = oldv;

            for (size_t i = 0; i < n; i++) {
                if (pares[i].antigo == oldv) { newv = pares[i].novo; break; }
            }

            pd[0] = newv; pd[1] = newv; pd[2] = newv; pd[3] = ps[3];
        }
    }
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Erro! É preciso passar a imagem ao executar!\n");
        printf("Uso: %s caminho/para/imagem.png\n", argv[0]);
        return 1;
    }
    const char* path = argv[1];

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        printf("Erro ao inicializar SDL: %s\n", SDL_GetError());
        return 1;
    }

    // SDL3_image NÃO precisa mais de IMG_Init/IMG_Quit.
    SDL_Surface* initial_img = IMG_Load(path);
    if (!initial_img) {
        printf("Erro ao carregar a imagem: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Surface* img = SDL_ConvertSurface(initial_img, SDL_PIXELFORMAT_RGBA32);
    if (!img) {
        printf("Erro na conversão para RGBA32: %s\n", SDL_GetError());
        SDL_DestroySurface(initial_img);
        SDL_Quit();
        return 1;
    }
    SDL_DestroySurface(initial_img);

    printf("Surface: w=%d, h=%d, pitch=%d, format=%u, flags=%u\n",
           img->w, img->h, img->pitch, img->format, img->flags);

    // --- Passo 1: garantir cinza (se não estiver) ---
    if (!SDL_LockSurface(img)) {
        printf("Erro ao travar surface para escala de cinza: %s\n", SDL_GetError());
        SDL_DestroySurface(img);
        SDL_Quit();
        return 1;
    }
    int escalaCinza = verifica_se_imagem_e_cinza(img);
    if (escalaCinza == 0) {
        aplicar_escala_de_cinza(img);
        printf("Imagem convertida para escala de cinza.\n");
    } else if (escalaCinza == 1) {
        printf("Imagem já está em escala de cinza.\n");
    } else {
        printf("Aviso: formato inesperado; esperava RGBA32.\n");
    }
    SDL_UnlockSurface(img);

    if (!SDL_SaveBMP(img, "saida.bmp")) {
        printf("Erro ao salvar 'saida.bmp': %s\n", SDL_GetError());
    } else {
        printf("Processada: %dx%d, salva em 'saida.bmp'\n", img->w, img->h);
    }

    // --- Passo 2: criar matriz de equalização a partir da imagem ---
    if (!SDL_LockSurface(img)) {
        printf("Erro ao travar surface para criar matriz: %s\n", SDL_GetError());
        SDL_DestroySurface(img);
        SDL_Quit();
        return 1;
    }
    ParIntensidade* matriz = NULL;
    size_t linhas = criar_matriz_mapeamento_por_imagem(img, &matriz);
    SDL_UnlockSurface(img);

    if (linhas == 0 || !matriz) {
        printf("Erro: não foi possível criar a matriz de mapeamento.\n");
        SDL_DestroySurface(img);
        SDL_Quit();
        return 1;
    }
    printf("Matriz de mapeamento criada com %zu linhas (colunas: antigo|novo).\n", linhas);

    // --- Passo 3: criar destino e aplicar equalização ---
    SDL_Surface* eq = SDL_CreateSurface(img->w, img->h, SDL_PIXELFORMAT_RGBA32);
    if (!eq) {
        printf("Erro ao criar surface destino para equalização: %s\n", SDL_GetError());
        free(matriz);
        SDL_DestroySurface(img);
        SDL_Quit();
        return 1;
    }

    if (!SDL_LockSurface(img)) {
        printf("Erro ao travar 'img' para equalização: %s\n", SDL_GetError());
        free(matriz);
        SDL_DestroySurface(eq);
        SDL_DestroySurface(img);
        SDL_Quit();
        return 1;
    }
    if (!SDL_LockSurface(eq)) {
        printf("Erro ao travar 'eq' para equalização: %s\n", SDL_GetError());
        SDL_UnlockSurface(img);
        free(matriz);
        SDL_DestroySurface(eq);
        SDL_DestroySurface(img);
        SDL_Quit();
        return 1;
    }

    (void)equalizar_com_matriz_linear(img, eq, matriz, linhas);

    SDL_UnlockSurface(eq);
    SDL_UnlockSurface(img);

    if (!SDL_SaveBMP(eq, "saida_eq.bmp")) {
        printf("Erro ao salvar 'saida_eq.bmp': %s\n", SDL_GetError());
    } else {
        printf("Equalizada salva em 'saida_eq.bmp'\n");
    }

    // === [B] GUI - Duas janelas (Item 3) ===
    int img_w = img->w, img_h = img->h;

    // Cria janela principal
    SDL_Window* win_main = SDL_CreateWindow("Proj1 - Janela Principal", img_w, img_h, SDL_WINDOW_RESIZABLE);
    if (!win_main) {
        printf("Erro ao criar janela principal: %s\n", SDL_GetError());
        free(matriz);
        SDL_DestroySurface(eq);
        SDL_DestroySurface(img);
        SDL_Quit();
        return 1;
    }
    SDL_SetWindowPosition(win_main, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

    // (opcional) entrar em tela cheia imediatamente
    SDL_SetWindowFullscreen(win_main, true);

    // Renderer principal + textura da imagem cinza
    SDL_Renderer* ren_main = SDL_CreateRenderer(win_main, NULL);
    if (!ren_main) {
        printf("Erro ao criar renderer principal: %s\n", SDL_GetError());
        SDL_DestroyWindow(win_main);
        free(matriz);
        SDL_DestroySurface(eq);
        SDL_DestroySurface(img);
        SDL_Quit();
        return 1;
    }
    SDL_Texture* tex_gray = SDL_CreateTextureFromSurface(ren_main, img);
    if (!tex_gray) {
        printf("Erro ao criar textura (gray): %s\n", SDL_GetError());
        SDL_DestroyRenderer(ren_main);
        SDL_DestroyWindow(win_main);
        free(matriz);
        SDL_DestroySurface(eq);
        SDL_DestroySurface(img);
        SDL_Quit();
        return 1;
    }
    // Mantém proporção automaticamente em qualquer tamanho/ fullscreen (letterbox)
    SDL_SetTextureScaleMode(tex_gray, SDL_SCALEMODE_LINEAR);
    SDL_SetRenderLogicalPresentation(ren_main, img_w, img_h, SDL_LOGICAL_PRESENTATION_LETTERBOX);

    // Janela secundária (tamanho fixo) ao lado da principal
    const int SEC_W = 420, SEC_H = 380;
    int mx, my; // posição da principal
    SDL_GetWindowPosition(win_main, &mx, &my);
    int sx = mx + img_w + 10; // 10px à direita da principal
    int sy = my;

    SDL_Window* win_side = SDL_CreateWindow("Proj1 - Janela Secundária", SEC_W, SEC_H, SDL_WINDOW_UTILITY);
    if (!win_side) {
        printf("Erro ao criar janela secundária: %s\n", SDL_GetError());
        SDL_DestroyTexture(tex_gray);
        SDL_DestroyRenderer(ren_main);
        SDL_DestroyWindow(win_main);
        free(matriz);
        SDL_DestroySurface(eq);
        SDL_DestroySurface(img);
        SDL_Quit();
        return 1;
    }
    SDL_SetWindowPosition(win_side, sx, sy);
    (void)SDL_SetWindowParent(win_side, win_main); // ok se falhar

    SDL_Renderer* ren_side = SDL_CreateRenderer(win_side, NULL);
    if (!ren_side) {
        printf("Erro ao criar renderer secundário: %s\n", SDL_GetError());
        SDL_DestroyWindow(win_side);
        SDL_DestroyTexture(tex_gray);
        SDL_DestroyRenderer(ren_main);
        SDL_DestroyWindow(win_main);
        free(matriz);
        SDL_DestroySurface(eq);
        SDL_DestroySurface(img);
        SDL_Quit();
        return 1;
    }

    // === [H-SETUP] Pré-cálculo do histograma/estatísticas da imagem cinza ===
    uint32_t hist[256];
    double media = 0.0, desvio = 0.0;
    if (calcular_histograma(img, hist, &media, &desvio) != 0) {
        printf("Aviso: falha ao calcular histograma.\n");
    }
    uint32_t hist_max = 0;
    for (int i = 0; i < 256; i++) if (hist[i] > hist_max) hist_max = hist[i];
    if (hist_max == 0) hist_max = 1; // evita divisão por zero
    printf("Estatísticas - média: %.2f, desvio-padrão: %.2f\n", media, desvio);

    // título com tamanho da imagem
    char title_buf[128];
    snprintf(title_buf, sizeof(title_buf), "Imagem: %dx%dpix", img_w, img_h);
    SDL_SetWindowTitle(win_main, title_buf);

    // === Loop de eventos e renderização ===
    bool running = true;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) {
                running = false;
            } else if (e.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
                running = false; // qualquer janela fechada encerra
            }
            // (itens 5 e 6 entram aqui depois)
        }

        // ----- Render janela principal (SDL faz letterbox proporcional) -----
        SDL_SetRenderDrawColor(ren_main, 20, 20, 20, 255);
        SDL_RenderClear(ren_main);
        SDL_RenderTexture(ren_main, tex_gray, NULL, NULL);
        SDL_RenderPresent(ren_main);

        // ----- Render janela secundária: histograma -----
        SDL_SetRenderDrawColor(ren_side, 32, 36, 48, 255);
        SDL_RenderClear(ren_side);

        // Área do histograma
        SDL_FRect pane = {16, 16, SEC_W - 32.0f, SEC_H - 32.0f};
        SDL_SetRenderDrawColor(ren_side, 80, 160, 220, 255);
        SDL_RenderRect(ren_side, &pane);

        // Margens internas
        float left   = pane.x + 10.0f;
        float right  = pane.x + pane.w - 10.0f;
        float top    = pane.y + 10.0f;
        float bottom = pane.y + pane.h - 50.0f; // espaço para barra inferior
        float w_plot = right - left;
        float h_plot = bottom - top;

        // Linhas-guia 25/50/75%
        SDL_SetRenderDrawColor(ren_side, 70, 80, 100, 255);
        for (int g = 1; g <= 3; g++) {
            float gy = top + (h_plot * (float)g / 4.0f);
            SDL_FRect gl = { left, gy, w_plot, 1.0f };
            SDL_RenderFillRect(ren_side, &gl);
        }

        // 256 barras
        float barW = w_plot / 256.0f;
        if (barW < 1.0f) barW = 1.0f;
        for (int i = 0; i < 256; i++) {
            float norm = (float)hist[i] / (float)hist_max; // 0..1
            float bh   = norm * h_plot;
            float bx   = left + i * (w_plot / 256.0f);
            float by   = bottom - bh;

            SDL_FRect br = { bx, by, barW, bh };
            SDL_SetRenderDrawColor(ren_side, 160, 200, 255, 255);
            SDL_RenderFillRect(ren_side, &br);
        }

        // Linha da média
        float meanX = left + (float)(media / 255.0f) * w_plot;
        SDL_SetRenderDrawColor(ren_side, 255, 200, 80, 255);
        SDL_FRect meanLine = { meanX - 1.0f, top, 2.0f, h_plot };
        SDL_RenderFillRect(ren_side, &meanLine);

        // Barra decorativa inferior
        SDL_FRect bar = {pane.x + 8, pane.y + pane.h - 40, pane.w - 16, 24};
        SDL_SetRenderDrawColor(ren_side, 160, 200, 255, 255);
        SDL_RenderFillRect(ren_side, &bar);

        SDL_RenderPresent(ren_side);

        SDL_Delay(16); // ~60 FPS
    }

    // === Limpeza ===
    SDL_DestroyRenderer(ren_side);
    SDL_DestroyWindow(win_side);
    SDL_DestroyTexture(tex_gray);
    SDL_DestroyRenderer(ren_main);
    SDL_DestroyWindow(win_main);

    free(matriz);
    SDL_DestroySurface(eq);
    SDL_DestroySurface(img);
    SDL_Quit();
    return 0;
}