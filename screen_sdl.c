#include "screen.h"

#include <SDL2/SDL.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct sdl_impl {
  SDL_Window *window;
  SDL_Renderer *renderer;
  SDL_Texture *texture;
};

void screen_free(struct screen *s) {
  if (s == NULL) return;
  struct sdl_impl *impl = s->impl;

  if (s->fb != NULL && impl != NULL) {
    screen_clear(s);
  }

  if (impl) {
    if (impl->texture) SDL_DestroyTexture(impl->texture);
    if (impl->renderer) SDL_DestroyRenderer(impl->renderer);
    if (impl->window) SDL_DestroyWindow(impl->window);
    free(impl);
  }

  free(s->fb);
  SDL_Quit();
  free(s);
}

struct screen *screen_new(void) {
  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
    return NULL;
  }

  struct screen *s = malloc(sizeof(struct screen));
  struct sdl_impl *impl = malloc(sizeof(struct sdl_impl));
  s->impl = impl;
  // Hardcode PAL-like screen
  s->width = 720;
  s->height = 576;
  s->bpp = 32;
  s->stride = s->width * 4;

  impl->window = SDL_CreateWindow("picrt", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                  s->width, s->height, 0);
  if (!impl->window) {
    fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
    screen_free(s);
    return NULL;
  }

  impl->renderer = SDL_CreateRenderer(impl->window, -1,
                                      SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!impl->renderer) {
    fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError());
    screen_free(s);
    return NULL;
  }

  impl->texture = SDL_CreateTexture(impl->renderer, SDL_PIXELFORMAT_ARGB8888,
                                    SDL_TEXTUREACCESS_STREAMING, s->width, s->height);
  if (!impl->texture) {
    fprintf(stderr, "SDL_CreateTexture: %s\n", SDL_GetError());
    screen_free(s);
    return NULL;
  }

  s->fb = calloc(1, s->stride * s->height);

  printf("screen (SDL): %dx%d, %d bpp, stride %d\n", s->width, s->height, s->bpp, s->stride);
  return s;
}

void screen_set_pixel(struct screen *s, int x, int y, unsigned char val) {
  if (x < 0 || x >= s->width || y < 0 || y >= s->height) return;
  unsigned char *px = s->fb + y * s->stride + x * 4;
  px[0] = val; px[1] = val; px[2] = val; px[3] = 0xFF;
}

void screen_flip(struct screen *s) {
  struct sdl_impl *impl = s->impl;
  SDL_UpdateTexture(impl->texture, NULL, s->fb, s->stride);
  SDL_RenderCopy(impl->renderer, impl->texture, NULL, NULL);
  SDL_RenderPresent(impl->renderer);

  SDL_Event e;
  while (SDL_PollEvent(&e)) {
    if (e.type == SDL_QUIT) {
      extern volatile sig_atomic_t running;
      running = 0;
    }
  }
}

void screen_clear(struct screen* s) {
    memset(s->fb, 0, s->stride * s->height);
}
