#include <math.h>
#include <signal.h>
#include <stdbool.h>
#include <string.h>
#include <sys/prctl.h>
#include <time.h>

#include "screen.h"

static volatile sig_atomic_t running = 1;

static void sighandler(int sig) {
    (void)sig;
    running = 0;
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

int main(void) {
    signal(SIGINT, sighandler);
    signal(SIGTERM, sighandler);
    signal(SIGHUP, sighandler);
    // Monitor when parent is killed, so we can run over ssh and exit when session closes
    prctl(PR_SET_PDEATHSIG, SIGTERM);

    struct Screen* s = screen_new();
    if (s == NULL) {
      return 1;
    }

    struct timespec frame = { .tv_sec = 0, .tv_nsec = 33333333 }; /* ~30fps */
    double t = 0.0;

    while (running) {
      render_lissajous(s, t);
      t += 0.02;
      nanosleep(&frame, NULL);
    }

    memset(s->fb, 0, s->stride * s->height);
    screen_free(s);
    printf("\nClean exit.\n");
    return 0;
}
