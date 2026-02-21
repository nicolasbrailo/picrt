#include <stddef.h>
#include <stdio.h>
#include <jpeglib.h>

#include <math.h>
#include <signal.h>
#include <stdbool.h>
#include <string.h>
#include <sys/prctl.h>
#include <time.h>

#include "img_client/img_client.h"
#include "screen.h"

static volatile sig_atomic_t running = 1;

static void sighandler(int sig) {
    (void)sig;
    running = 0;
}

static void render_jpeg(struct Screen* s, const char* path, double gamma) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        perror(path);
        return;
    }

    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);
    jpeg_stdio_src(&cinfo, f);
    jpeg_read_header(&cinfo, TRUE);

    unsigned int denoms[] = {1, 2, 4, 8};
    cinfo.scale_num = 1;
    cinfo.scale_denom = 1;
    for (int i = 0; i < 4; i++) {
        if ((int)(cinfo.image_width / denoms[i]) >= s->width &&
            (int)(cinfo.image_height / denoms[i]) >= s->height) {
            cinfo.scale_denom = denoms[i];
        }
    }

    jpeg_start_decompress(&cinfo);

    int off_x = ((int)cinfo.output_width - s->width) / 2;
    int off_y = ((int)cinfo.output_height - s->height) / 2;
    if (off_x < 0) off_x = 0;
    if (off_y < 0) off_y = 0;

    int row_stride = cinfo.output_width * cinfo.output_components;
    unsigned char *row = malloc(row_stride);
    int y = 0;

    memset(s->fb, 0, s->stride * s->height);

    while (cinfo.output_scanline < cinfo.output_height) {
        jpeg_read_scanlines(&cinfo, &row, 1);
        int sy = y - off_y;
        if (sy >= 0 && sy < s->height) {
            for (int x = 0; x < s->width && (x + off_x) < (int)cinfo.output_width; x++) {
                int src = (x + off_x) * cinfo.output_components;
                unsigned char r = row[src];
                unsigned char g = (cinfo.output_components >= 3) ? row[src + 1] : r;
                unsigned char b = (cinfo.output_components >= 3) ? row[src + 2] : r;
                unsigned char val = (unsigned char)(255.0 * pow((0.299 * r + 0.587 * g + 0.114 * b) / 255.0, gamma));
                screen_set_pixel(s, x, sy, val);
            }
        }
        y++;
    }

    free(row);
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    fclose(f);
}

static void render_jpeg_from_mem(struct Screen* s, const unsigned char* data, size_t sz, double gamma) {
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);
    jpeg_mem_src(&cinfo, data, sz);
    jpeg_read_header(&cinfo, TRUE);

    unsigned int denoms[] = {1, 2, 4, 8};
    cinfo.scale_num = 1;
    cinfo.scale_denom = 1;
    for (int i = 0; i < 4; i++) {
        if ((int)(cinfo.image_width / denoms[i]) >= s->width &&
            (int)(cinfo.image_height / denoms[i]) >= s->height) {
            cinfo.scale_denom = denoms[i];
        }
    }

    jpeg_start_decompress(&cinfo);
    printf("JPEG: %dx%d -> %dx%d (1/%d), screen %dx%d\n",
           cinfo.image_width, cinfo.image_height,
           cinfo.output_width, cinfo.output_height,
           cinfo.scale_denom, s->width, s->height);

    int off_x = ((int)cinfo.output_width - s->width) / 2;
    int off_y = ((int)cinfo.output_height - s->height) / 2;
    if (off_x < 0) off_x = 0;
    if (off_y < 0) off_y = 0;

    int row_stride = cinfo.output_width * cinfo.output_components;
    unsigned char *row = malloc(row_stride);
    int y = 0;

    memset(s->fb, 0, s->stride * s->height);

    while (cinfo.output_scanline < cinfo.output_height) {
        jpeg_read_scanlines(&cinfo, &row, 1);
        int sy = y - off_y;
        if (sy >= 0 && sy < s->height) {
            for (int x = 0; x < s->width && (x + off_x) < (int)cinfo.output_width; x++) {
                int src = (x + off_x) * cinfo.output_components;
                unsigned char r = row[src];
                unsigned char g = (cinfo.output_components >= 3) ? row[src + 1] : r;
                unsigned char b = (cinfo.output_components >= 3) ? row[src + 2] : r;
                unsigned char val = (unsigned char)(255.0 * pow((0.299 * r + 0.587 * g + 0.114 * b) / 255.0, gamma));
                screen_set_pixel(s, x, sy, val);
            }
        }
        y++;
    }

    free(row);
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
}


static void on_image_ready(void* usr, const unsigned char* data, size_t sz) {
    struct Screen* s = usr;
    render_jpeg_from_mem(s, data, sz, 0.1);
}

void render_lissajous(struct Screen* s, double t) {
    memset(s->fb, 0, s->stride * s->height);

    int cx = s->width / 2 + (int)(40 * sin(t * 0.5));
    int cy = s->height / 2;
    int rx = cx - 20;
    int ry = cy - 20;

    double a = 3.0, b = 2.0, delta = M_PI * sin(t * 0.8);
    int trail = 2000;

    for (int i = 0; i < trail; i++) {
        double p = t + (double)i * (2.0 * M_PI / trail);
        int x = cx + (int)(rx * sin(a * p + delta));
        int y = cy + (int)(ry * sin(b * p));

        unsigned char brightness = 128 + (unsigned char)(127.0 * i / trail);

        for (int dy = -1; dy <= 1; dy++)
            for (int dx = -1; dx <= 1; dx++)
                screen_set_pixel(s, x + dx, y + dy, brightness);
    }
}

int main(int argc, char* argv[]) {
    signal(SIGINT, sighandler);
    signal(SIGTERM, sighandler);
    signal(SIGHUP, sighandler);
    // Monitor when parent is killed, so we can run over ssh and exit when session closes
    prctl(PR_SET_PDEATHSIG, SIGTERM);

    struct Screen* s = screen_new();
    if (s == NULL) {
      return 1;
    }

    if (argc > 1) {
      render_jpeg(s, argv[1], /*gamma=*/.1);
      printf("Rendered %s. Press Ctrl-C to exit.\n", argv[1]);
      while (running) {
        pause();
      }
    } else {
      struct img_client_ctx* img_render = img_client_init(s->width, s->height, "http://bati.casa:5000/", on_image_ready, s);
      if (img_render) {
        img_client_run(img_render, &running);
        img_client_free(img_render);
      }
    }

    memset(s->fb, 0, s->stride * s->height);
    screen_free(s);
    printf("\nClean exit.\n");
    return 0;
}
