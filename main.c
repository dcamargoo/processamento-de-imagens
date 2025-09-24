// Universidade Presbiteriana Mackenzie – Computação Visual – Proj1 (SDL3)
// Build (Ubuntu):
//   gcc -std=c99 -O2 -Wall -Wextra -o main.exe main.c   $(pkg-config --cflags --libs sdl3 sdl3-image sdl3-ttf) -lm
// Run:
//   ./main caminho/para/imagem.png

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <errno.h>
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>

// Função para posicionar a janela 'win_side' ao lado da 'win_main'
static void place_side_window(SDL_Window *win_main, SDL_Window *win_side, int side_w, int side_h) {
    if (!win_main || !win_side) return;

    int mx, my, mw, mh;
    SDL_GetWindowPosition(win_main, &mx, &my);
    SDL_GetWindowSize(win_main, &mw, &mh);

    SDL_DisplayID display = SDL_GetDisplayForWindow(win_main);
    SDL_Rect usable = (SDL_Rect){0,0,0,0};
    SDL_GetDisplayUsableBounds(display, &usable);

    int x = mx + mw + 16;   // ao lado direito da principal
    int y = my;

    // se não couber à direita, tenta à esquerda
    if (x + side_w > usable.x + usable.w) x = mx - side_w - 16;

    // clamp nas bordas úteis
    if (x < usable.x) x = usable.x;
    if (y < usable.y) y = usable.y;
    if (y + side_h > usable.y + usable.h) y = usable.y + usable.h - side_h;

    SDL_SetWindowPosition(win_side, x, y);
    SDL_ShowWindow(win_side);
}


//Gera o histograma e conta o total de pixels
static void calcular_histograma(SDL_Surface* img, uint32_t hist[256], uint64_t* total_pixels) {
    memset(hist, 0, 256 * sizeof(uint32_t));
    *total_pixels = 0;

    if (!img || img->format != SDL_PIXELFORMAT_RGBA32) return;

    int w = img->w, h = img->h, pitch = img->pitch;
    uint8_t* base = (uint8_t*)img->pixels;

    for (int y = 0; y < h; y++) {
        uint8_t* row = base + y * pitch;
        for (int x = 0; x < w; x++) {
            uint8_t* p = row + x * 4;
            uint8_t Y = p[0]; // RGBA32 e imagem em cinza: R=G=B
            hist[Y]++;
            (*total_pixels)++;
        }
    }
}
//Calcula média e desvio padrão do histograma
static void estatisticas_do_histograma(const uint32_t hist[256], uint64_t total,
                                       double* media, double* desvio) {
    if (total == 0) { *media = 0.0; *desvio = 0.0; return; }
    double soma = 0.0;
    for (int i = 0; i < 256; i++) soma += i * (double)hist[i];
    double mu = soma / (double)total;
    double var = 0.0;
    for (int i = 0; i < 256; i++) {
        double d = (double)i - mu;
        var += d * d * (double)hist[i];
    }
    var /= (double)total;
    *media = mu;
    *desvio = sqrt(var);
}
//Descrição da media da imagem baseado no histograma
static const char* class_luminosidade(double media) {
    if (media < 85.0) return "escura";
    if (media < 170.0) return "media";
    return "clara";
}
//Descrição do desvio da imagem baseado no histograma
static const char* class_contraste(double desvio) {
    if (desvio > 60.0) return "alto";
    if (desvio > 30.0) return "medio";
    return "baixo";
}

// Desenha histograma em uma área (retângulo) usando barras
static void render_histograma(SDL_Renderer* r, SDL_FRect area, const uint32_t hist[256]) {
    SDL_SetRenderDrawColor(r, 30, 30, 40, 255);
    SDL_RenderFillRect(r, &area);

    float pad = 10.0f;
    SDL_FRect plot = (SDL_FRect){ area.x + pad, area.y + pad, area.w - 2*pad, area.h - 2*pad };

    SDL_SetRenderDrawColor(r, 80, 80, 120, 255);
    SDL_RenderRect(r, &plot);

    uint32_t maxv = 1;
    for (int i = 0; i < 256; i++) if (hist[i] > maxv) maxv = hist[i];

    float wbar = plot.w / 256.0f;
    for (int i = 0; i < 256; i++) {
        float x = plot.x + i * wbar;
        float h = (hist[i] / (float)maxv) * (plot.h - 1.0f);
        SDL_FRect bar = (SDL_FRect){ x, plot.y + plot.h - h, wbar, h };
        SDL_SetRenderDrawColor(r, 120, 180, 240, 255);
        SDL_RenderFillRect(r, &bar);
    }
}

static TTF_Font* g_ui_font = NULL;

// Desenha um botão e retorna 1 para mudar de cor caso o mouse esteja sobre ele
static int render_botao(SDL_Renderer* r, SDL_FRect rect, int hovered, int pressed, const char* rotulo_curto) {
    if (pressed) SDL_SetRenderDrawColor(r, 30, 70, 150, 255);
    else if (hovered) SDL_SetRenderDrawColor(r, 60, 120, 220, 255);
    else SDL_SetRenderDrawColor(r, 40, 90, 190, 255);

    SDL_RenderFillRect(r, &rect);

    SDL_SetRenderDrawColor(r, 15, 35, 70, 255);
    SDL_RenderRect(r, &rect);

//Desenha o botão, define a cor e calcula o centro
    SDL_SetRenderDrawColor(r, 230, 230, 240, 255);
    float cx = rect.x + rect.w * 0.5f;
    float cy = rect.y + rect.h * 0.5f;
    if (g_ui_font && rotulo_curto && rotulo_curto[0] != '\0') {
        SDL_Color fg = {240, 244, 255, 255};
        SDL_Surface* surf = TTF_RenderText_Blended(g_ui_font, rotulo_curto, strlen(rotulo_curto), fg);
        if (surf) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
            if (tex) {
                SDL_FRect dst = {
                    rect.x + (rect.w - (float)surf->w) * 0.5f,
                    rect.y + (rect.h - (float)surf->h) * 0.5f,
                    (float)surf->w,
                    (float)surf->h
                };
                SDL_RenderTexture(r, tex, NULL, &dst);
                SDL_DestroyTexture(tex);
            }
            SDL_DestroySurface(surf);
        }
    } else {
        SDL_RenderLine(r, cx - 20, cy, cx + 20, cy);
        SDL_RenderLine(r, cx, cy - 6, cx, cy + 6);
    }
    (void)rotulo_curto;
    return hovered;
}

// Retorna 1 se estiver em escala de cinza e 0 se não
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
//quando a imagem não é cinza, aplica tons de cinza nela
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
//Equaliza tons de cinza por mapeamento
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
//Tratamento de erros
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

    
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "Erro: não foi possível abrir '%s' (%s)\n", path, strerror(errno));
        SDL_Quit();
        return 1;
    }
    fclose(f);

    SDL_ClearError();
    SDL_Surface* initial_img = IMG_Load(path);
    if (!initial_img) {
        fprintf(stderr,
                "Erro: o arquivo não é uma imagem suportada ou está corrompido.\nDetalhe: %s\n",
                SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Surface* img = SDL_ConvertSurface(initial_img, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(initial_img);
    if (!img) {
        fprintf(stderr, "Erro: falha ao converter para RGBA32: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    if (img->w <= 0 || img->h <= 0) {
        fprintf(stderr, "Erro: dimensões de imagem inválidas (%dx%d).\n", img->w, img->h);
        SDL_DestroySurface(img);
        SDL_Quit();
        return 1;
    }

    //garantir cinza na imagem
    if (!SDL_LockSurface(img)) {
        printf("Erro ao travar surface para escala de cinza: %s\n", SDL_GetError());
        SDL_DestroySurface(img);
        SDL_Quit();
        return 1;
    }
    int escalaCinza = verifica_se_imagem_e_cinza(img);
    if(escalaCinza == 0){
        aplicar_escala_de_cinza(img);
    }

    int w = img->w, h = img->h;
    SDL_UnlockSurface(img);

    //Cria matriz somente com intensidades presentes
    ParIntensidade* matriz = NULL;
    size_t linhas = criar_matriz_mapeamento_por_imagem(img, &matriz);
    SDL_UnlockSurface(img);

    if (linhas == 0 || !matriz) {
        printf("Erro: não foi possível criar a matriz de mapeamento.\n");
        SDL_DestroySurface(img); SDL_Quit();
        return 1;
    }

    SDL_Surface* eq = SDL_ConvertSurface(img, SDL_PIXELFORMAT_RGBA32);
    if (!eq) {
        printf("Erro ao criar cópia para equalização.\n");
        free(matriz);
        SDL_DestroySurface(img); SDL_Quit();
        return 1;
    }

    if (!SDL_LockSurface(img) || !SDL_LockSurface(eq)) {
        printf("Erro ao travar surfaces na equalização.\n");
        SDL_DestroySurface(eq);
        free(matriz);
        SDL_DestroySurface(img); SDL_Quit();
        return 1;
    }

    (void)equalizar_com_matriz_linear(img, eq, matriz, linhas);

    SDL_UnlockSurface(eq);
    SDL_UnlockSurface(img);

    {
        //Janela principal
        SDL_Window* win_main = SDL_CreateWindow("Proj1 - Principal (Imagem)",
                                                w, h, SDL_WINDOW_RESIZABLE);
        if (!win_main) {
            printf("Erro ao criar janela principal: %s\n", SDL_GetError());
            free(matriz);
            SDL_DestroySurface(eq);
            SDL_DestroySurface(img);
            SDL_Quit();
            return 1;
        }
        SDL_SetWindowPosition(win_main, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

        SDL_Renderer* ren_main = SDL_CreateRenderer(win_main, NULL);
        if (!ren_main) {
            printf("Erro ao criar renderer principal: %s\n", SDL_GetError());
            SDL_DestroyWindow(win_main);
            free(matriz); SDL_DestroySurface(eq); SDL_DestroySurface(img); SDL_Quit(); return 1;
        }

        // texturas para original e equalizada
        SDL_Texture* tex_orig = SDL_CreateTextureFromSurface(ren_main, img);
        SDL_Texture* tex_eq   = SDL_CreateTextureFromSurface(ren_main, eq);
        SDL_Texture* tex_atual = tex_orig;

        // 2) Janela secundária (NORMAL) ao lado
        const int SEC_W = 480;
        const int SEC_H = 560;

        SDL_Window* win_sec = SDL_CreateWindow("Proj1 - Secundaria (Histograma)", SEC_W, SEC_H, 0);
        if (!win_sec) {
            printf("Erro ao criar janela secundária: %s\n", SDL_GetError());
            SDL_DestroyTexture(tex_orig); SDL_DestroyTexture(tex_eq);
            SDL_DestroyRenderer(ren_main); SDL_DestroyWindow(win_main);
            free(matriz); SDL_DestroySurface(eq); SDL_DestroySurface(img); SDL_Quit(); return 1;
        }
        place_side_window(win_main, win_sec, SEC_W, SEC_H);

        SDL_Renderer* ren_sec = SDL_CreateRenderer(win_sec, NULL);
        if (!ren_sec) {
            printf("Erro ao criar renderer secundário: %s\n", SDL_GetError());
            SDL_DestroyWindow(win_sec);
            SDL_DestroyTexture(tex_orig); SDL_DestroyTexture(tex_eq);
            SDL_DestroyRenderer(ren_main); SDL_DestroyWindow(win_main);
            free(matriz); SDL_DestroySurface(eq); SDL_DestroySurface(img); SDL_Quit(); return 1;
        }

        if (TTF_Init()) {
            g_ui_font = TTF_OpenFont("DejaVuSans.ttf", 18);
            if (!g_ui_font) g_ui_font = TTF_OpenFont("C:\\Windows\\Fonts\\arial.ttf", 18);
        }

        // Estado do botão e histograma
        int equalizado_on = 0; // começa mostrando original
        uint32_t hist[256];
        uint64_t total = 0;
        calcular_histograma(img, hist, &total); // histograma da imagem atual (original)
        double media = 0.0, desvio = 0.0;
        estatisticas_do_histograma(hist, total, &media, &desvio);

        // Atualiza título com as infos
        char titulo_sec[256];
        snprintf(titulo_sec, sizeof(titulo_sec),
                 "Hist: media=%.1f (%s), desvio=%.1f (contraste %s)  |  Botao: Equalizado",
                 media, class_luminosidade(media), desvio, class_contraste(desvio));
        SDL_SetWindowTitle(win_sec, titulo_sec);

        // Áreas na secundária: histograma e botão
        SDL_FRect area_hist = (SDL_FRect){ 16, 16, SEC_W - 32, SEC_H - 16 - 120 };
        SDL_FRect area_btn  = (SDL_FRect){ SEC_W/2.0f - 120.0f, SEC_H - 60.0f, 240.0f, 40.0f };

        int running = 1;
        int btn_hover = 0, btn_pressed = 0;

        while (running) {
            SDL_Event e;
            while (SDL_PollEvent(&e)) {
                if (e.type == SDL_EVENT_QUIT) running = 0;
                else if (e.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) running = 0;

                // Reposiciona a janela do histograma se a principal se mover/trocar tamanho
                else if ((e.type == SDL_EVENT_WINDOW_MOVED || e.type == SDL_EVENT_WINDOW_RESIZED) &&
                          e.window.windowID == SDL_GetWindowID(win_main)) {
                    place_side_window(win_main, win_sec, SEC_W, SEC_H);
                }
                else if (e.type == SDL_EVENT_KEY_DOWN) {
                    // ESC sai; S salva o que está visível AGORA
                    if (e.key.scancode == SDL_SCANCODE_ESCAPE) {
                        running = 0;
                    } else if (e.key.scancode == SDL_SCANCODE_S) {
                        // Salva diretamente a imagem em memória (sem passar pelo renderer)
                        SDL_Surface* atual = equalizado_on ? eq : img;
                        if (IMG_SavePNG(atual, "output_image.png") != 0) {
                            SDL_Log("Imagem salva como 'output_image.png'");
                        } else {
                            SDL_Log("Falha ao salvar 'output_image.png': %s", SDL_GetError());
                        }
                    }

                } else if (e.type == SDL_EVENT_MOUSE_MOTION) {
                    if (e.motion.windowID == SDL_GetWindowID(win_sec)) {
                        float mx = (float)e.motion.x;
                        float my = (float)e.motion.y;
                        btn_hover = (mx >= area_btn.x && mx <= area_btn.x + area_btn.w &&
                                     my >= area_btn.y && my <= area_btn.y + area_btn.h);
                    }
                } else if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                    if (e.button.windowID == SDL_GetWindowID(win_sec) && e.button.button == SDL_BUTTON_LEFT) {
                        float mx = (float)e.button.x, my = (float)e.button.y;
                        if (mx >= area_btn.x && mx <= area_btn.x + area_btn.w &&
                            my >= area_btn.y && my <= area_btn.y + area_btn.h) {
                            btn_pressed = 1;
                        }
                    }
                } else if (e.type == SDL_EVENT_MOUSE_BUTTON_UP) {
                    if (btn_pressed && e.button.windowID == SDL_GetWindowID(win_sec) &&
                        e.button.button == SDL_BUTTON_LEFT) {
                        float mx = (float)e.button.x, my = (float)e.button.y;
                        int inside = (mx >= area_btn.x && mx <= area_btn.x + area_btn.w &&
                                      my >= area_btn.y && my <= area_btn.y + area_btn.h);
                        btn_pressed = 0;
                        if (inside) {
                            // Toggle entre imagens original e equalizada
                            equalizado_on = !equalizado_on;
                            tex_atual = equalizado_on ? tex_eq : tex_orig;

                            // Recalcula histograma da imagem atualmente exibida
                            if (equalizado_on) calcular_histograma(eq, hist, &total);
                            else calcular_histograma(img, hist, &total);
                            estatisticas_do_histograma(hist, total, &media, &desvio);

                            snprintf(titulo_sec, sizeof(titulo_sec),
                                     "Hist: media=%.1f (%s), desvio=%.1f (contraste %s)  |  Botao: %s",
                                     media, class_luminosidade(media), desvio, class_contraste(desvio),
                                     equalizado_on ? "Original" : "Equalizado");
                            SDL_SetWindowTitle(win_sec, titulo_sec);
                        }
                    }
                }
            }

            // Render principal
            int vw, vh;
            SDL_GetWindowSize(win_main, &vw, &vh);
            SDL_SetRenderDrawColor(ren_main, 12, 12, 12, 255);
            SDL_RenderClear(ren_main);
            SDL_FRect dst = (SDL_FRect){0, 0, (float)vw, (float)vh};
            SDL_RenderTexture(ren_main, tex_atual, NULL, &dst);
            SDL_RenderPresent(ren_main);

            // Render secundária: histograma + textos + botão
            SDL_SetRenderDrawColor(ren_sec, 22, 22, 28, 255);
            SDL_RenderClear(ren_sec);
            render_histograma(ren_sec, area_hist, hist);

            if (g_ui_font) {
                char l1[128], l2[128];
                snprintf(l1, sizeof(l1), "Média: %.1f (%s)", media, class_luminosidade(media));
                snprintf(l2, sizeof(l2), "Desvio padrão: %.1f (%s)", desvio, class_contraste(desvio));
                SDL_Color fg = (SDL_Color){230,230,240,255};

                SDL_Surface* s1 = TTF_RenderText_Blended(g_ui_font, l1, strlen(l1), fg);
                if (s1) {
                    SDL_Texture* t1 = SDL_CreateTextureFromSurface(ren_sec, s1);
                    if (t1) {
                        SDL_FRect d1 = { area_hist.x, area_hist.y + area_hist.h + 8, (float)s1->w, (float)s1->h };
                        SDL_RenderTexture(ren_sec, t1, NULL, &d1);
                        SDL_DestroyTexture(t1);
                    }
                    SDL_DestroySurface(s1);
                }

                SDL_Surface* s2 = TTF_RenderText_Blended(g_ui_font, l2, strlen(l2), fg);
                if (s2) {
                    SDL_Texture* t2 = SDL_CreateTextureFromSurface(ren_sec, s2);
                    if (t2) {
                        SDL_FRect d2 = { area_hist.x, area_hist.y + area_hist.h + 8 + 22, (float)s2->w, (float)s2->h };
                        SDL_RenderTexture(ren_sec, t2, NULL, &d2);
                        SDL_DestroyTexture(t2);
                    }
                    SDL_DestroySurface(s2);
                }
            }

            render_botao(ren_sec, area_btn, btn_hover, btn_pressed, equalizado_on ? "Original" : "Equalizado");

            SDL_RenderPresent(ren_sec);

            SDL_Delay(16); // ~60 FPS
        }

        // Limpeza
        SDL_DestroyTexture(tex_orig);
        SDL_DestroyTexture(tex_eq);
        SDL_DestroyRenderer(ren_main);
        SDL_DestroyWindow(win_main);
        SDL_DestroyRenderer(ren_sec);
        SDL_DestroyWindow(win_sec);
    }

    free(matriz);
    SDL_DestroySurface(eq);
    SDL_DestroySurface(img);
    if (g_ui_font) TTF_CloseFont(g_ui_font);
    TTF_Quit();
    SDL_Quit();
    return 0;
}
