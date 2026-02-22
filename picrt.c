#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <jpeglib.h>

#include <math.h>
#include <signal.h>
#include <stdbool.h>
#include <string.h>
#include <sys/prctl.h>
#include <unistd.h>
#include <time.h>

#include "img_client/img_client.h"

#include "screen.h"

// Adjust gamma for CRT. This will be washed out in a modern display but should work
// okay for old CRTs
#define GAMMA .15

// Image server
#define IMG_SERVER_URL "http://bati.casa:5000/"

// Sleep between imgs
#define IMAGE_INTERVAL_SEC 10

// Delay per scanline during reveal (microseconds), used when rendering a picture to the screen.
// Without this, the picture is displayed immediately, which is obviously wrong because a CRT
// is old so it must be slow. 500us will be about 300ms to display an image, 1ms will be about half
// a second. Set to zero to disable effect.
#define SCANLINE_DELAY_US 1000

sig_atomic_t running = 1;

static void sighandler(int sig) {
    (void)sig;
    running = 0;
}

static void render_jpeg(struct screen* s, const char* path, double gamma) {
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

    screen_clear(s);
    while (cinfo.output_scanline < cinfo.output_height && running) {
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
            screen_flip(s);
            if (SCANLINE_DELAY_US) usleep(SCANLINE_DELAY_US);
        }
        y++;
    }

    free(row);
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    fclose(f);
}

static void render_jpeg_from_mem(struct screen* s, const unsigned char* data, size_t sz, double gamma) {
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

    screen_clear(s);
    while (cinfo.output_scanline < cinfo.output_height && running) {
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
            screen_flip(s);
            if (SCANLINE_DELAY_US) usleep(SCANLINE_DELAY_US);
        }
        y++;
    }

    free(row);
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
}


void render_lissajous(struct screen* s) {
    struct timespec frame = { .tv_sec = 0, .tv_nsec = 33333333 }; /* ~30fps */
    double t = 0;
    while (running) {
      {
        int cx = s->width / 2 + (int)(40 * sin(t * 0.5));
        int cy = s->height / 2;
        int rx = cx - 20;
        int ry = cy - 20;

        double a = 3.0, b = 2.0, delta = M_PI * sin(t * 0.8);
        int trail = 2000;

        screen_clear(s);
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

      screen_flip(s);
      t += 0.02;
      nanosleep(&frame, NULL);
    }
}

static void render_single_img(struct screen* s, const char* img_path) {
      render_jpeg(s, img_path, GAMMA);
      printf("Rendered %s. Press Ctrl-C to exit.\n", img_path);
      while (running) {
        screen_flip(s);
        usleep(16000);
      }
}

static void render_img_client(struct screen* s) {
      struct img_client_ctx* img_render = img_client_init(s->width, s->height, IMG_SERVER_URL);
      if (img_render) {
        time_t last_image = 0;  // show first image immediately
        while (running) {
          time_t now = time(NULL);
          if (now - last_image >= IMAGE_INTERVAL_SEC) {
            const unsigned char* data;
            size_t sz;
            if (img_client_get_image(img_render, &data, &sz)) {
              printf("Rendering image (%zu bytes)\n", sz);
              render_jpeg_from_mem(s, data, sz, GAMMA);
              last_image = now;
            }
          }
          screen_flip(s);
          usleep(50000);
        }
        img_client_free(img_render);
      }
}

int main(int argc, char* argv[]) {
    signal(SIGINT, sighandler);
    signal(SIGTERM, sighandler);
    signal(SIGHUP, sighandler);
    // Monitor when parent is killed, so we can run over ssh and exit when session closes
    prctl(PR_SET_PDEATHSIG, SIGTERM);

    struct screen* s = screen_new();
    if (s == NULL) {
      return 1;
    }

    char run_mode = 's';
    if (argc > 1 && argv[1][0] == '-') {
      run_mode = argv[1][1];
    }

    if (run_mode == 's') {
      render_img_client(s);
    } else if (run_mode == 'l') {
      render_lissajous(s);
    } else if (run_mode == 'f' && argc > 2) {
      render_single_img(s, argv[2]);
    } else {
      printf("%s [-s|-l|-f file] - Do something with a CRT\n", argv[0]);
      printf("  -s  Display from image server\n");
      printf("  -f  Display a picture. Provide path after -f.\n");
      printf("  -l  Render a Lissajous figure so your CRT looks sciency\n");
      printf("  -h  Help\n");
    }

    screen_free(s);
    printf("\nClean exit.\n");
    return 0;
}
