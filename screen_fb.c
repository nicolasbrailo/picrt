#include "screen.h"

#include <fcntl.h>
#include <linux/fb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

struct fb_impl {
  int fd;
};

void screen_free(struct screen* s) {
  if (s == NULL) return;
  struct fb_impl *impl = s->impl;

  if (s->fb != MAP_FAILED && impl != NULL && impl->fd > 0) {
    screen_clear(s);
  }

  if (s->fb != MAP_FAILED) {
    munmap(s->fb, s->stride * s->height);
  }

  if (impl) {
    if (impl->fd > 0) {
      close(impl->fd);
    }
    free(impl);
  }

  free(s);
}

struct screen* screen_new(void) {
    struct screen *s = malloc(sizeof(struct screen));
    if (!s) return NULL;
    s->fb = MAP_FAILED;

    struct fb_impl* impl = malloc(sizeof(struct fb_impl));
    s->impl = impl;
    if (!impl) { screen_free(s); return NULL; }

    impl->fd = open("/dev/fb0", O_RDWR);
    if (impl->fd < 0) {
        perror("open /dev/fb0");
        screen_free(s);
        return NULL;
    }

    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
    if (ioctl(impl->fd, FBIOGET_FSCREENINFO, &finfo) < 0 ||
        ioctl(impl->fd, FBIOGET_VSCREENINFO, &vinfo) < 0) {
        perror("ioctl");
        screen_free(s);
        return NULL;
    }

    s->width = vinfo.xres;
    s->height = vinfo.yres;
    s->bpp = vinfo.bits_per_pixel;
    s->stride = finfo.line_length;

    size_t size = s->stride * s->height;
    s->fb = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, impl->fd, 0);
    if (s->fb == MAP_FAILED) {
        perror("mmap");
        screen_free(s);
        return NULL;
    }

    printf("Screen: %dx%d, %d bpp, stride %d\n", s->width, s->height, s->bpp, s->stride);
    return s;
}

void screen_set_pixel(struct screen* s, int x, int y, unsigned char val) {
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

void screen_flip(struct screen* s) {
    (void)s;
}

void screen_clear(struct screen* s) {
    memset(s->fb, 0, s->stride * s->height);
}
