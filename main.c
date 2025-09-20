#include <stdio.h>
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

int main(int argc, char* argv[]) {

    if (argc < 2) { // verifica se foi recebida a imagem ao executar
        printf("Erro! É preciso passar a imagem ao executar!");
        return 1;
    }

    const char* path = argv[1];

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "Erro SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    (void)IMG_Init(0); 

    SDL_Surface* img = IMG_Load(path);
    if (!img) {
        fprintf(stderr, "Falha ao carregar '%s': %s\n", path, IMG_GetError());
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    printf("OK: '%s' carregada (%dx%d px)\n", path, img->w, img->h);

    SDL_DestroySurface(img);
    IMG_Quit();
    SDL_Quit();
    
    return 0;
}
