#pragma once

#include <fcntl.h>
#include <linux/fb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

struct Screen {
  int fd;
  unsigned char *fb;
  int width;
  int height;
  int bpp;
  int stride;
};

static inline void screen_free(struct Screen* s) {
  if (s == NULL) {
    return;
  }

  if (s->fb != MAP_FAILED) {
    munmap(s->fb, s->width * s->height);
  }

  if (s->fd > 0) {
    close(s->fd);
  }

  free(s);
}

static inline struct Screen* screen_new() {
    struct Screen *screen = malloc(sizeof(struct Screen));
    screen->fb = MAP_FAILED;

    screen->fd = open("/dev/fb0", O_RDWR);
    if (screen->fd < 0) {
        perror("open /dev/fb0");
        screen_free(screen);
        return NULL;
    }

    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
    if (ioctl(screen->fd, FBIOGET_FSCREENINFO, &finfo) < 0 ||
        ioctl(screen->fd, FBIOGET_VSCREENINFO, &vinfo) < 0) {
        perror("ioctl");
        screen_free(screen);
        return NULL;
    }

    screen->width = vinfo.xres;
    screen->height = vinfo.yres;
    screen->bpp = vinfo.bits_per_pixel;
    screen->stride = finfo.line_length;

    size_t size = screen->stride * screen->height;
    screen->fb = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, screen->fd, 0);
    if (screen->fb == MAP_FAILED) {
        perror("mmap");
        screen_free(screen);
        return NULL;
    }

    printf("Screen: %dx%d, %d bpp, stride %d\n", screen->width, screen->height, screen->bpp, screen->stride);
    return screen;
}

static inline void screen_set_pixel(struct Screen* s, int x, int y, unsigned char val) {
    if (x < 0 || x >= s->width || y < 0 || y >= s->height) return;
    if (s->bpp == 32) {
        unsigned char *px = s->fb + y * s->stride + x * 4;
        px[0] = val; px[1] = val; px[2] = val; px[3] = 0xFF;
    } else if (s->bpp == 16) {
        unsigned short g = (val >> 2) << 5;
        unsigned short rb = val >> 3;
        *(unsigned short *)(s->fb + y * s->stride + x * 2) = (rb << 11) | g | rb;
    }
}
