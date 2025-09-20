#include <stdio.h>
#include <stdint.h> // para usar o uint8_t
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

    SDL_UnlockSurface(img);

    if (SDL_SaveBMP(img, "saida.bmp")) {
        printf("Processada: %dx%d, salva em 'saida.bmp'\n", w, h);
    } else {
        printf("Erro: falha ao salvar 'saida.bmp'\n");
    }

    SDL_DestroySurface(img);
    IMG_Quit();
    SDL_Quit();
    return 0;
}
