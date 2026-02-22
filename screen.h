#pragma once

struct screen {
  void *impl;
  unsigned char *fb;
  int width;
  int height;
  int bpp;
  int stride;
};

struct screen* screen_new(void);
void screen_free(struct screen* s);
void screen_set_pixel(struct screen* s, int x, int y, unsigned char val);
void screen_flip(struct screen* s);
void screen_clear(struct screen* s);
