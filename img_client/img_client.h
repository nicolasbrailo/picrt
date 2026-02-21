#pragma once

#include <signal.h>
#include <stddef.h>

struct img_client_ctx;

/**
 * Callback invoked when a new image is ready to render.
 * data/sz is the raw JPEG in memory. usr is the user pointer passed to init.
 */
typedef void (*image_ready_cb)(void* usr, const unsigned char* data, size_t sz);

struct img_client_ctx* img_client_init(int screen_w, int screen_h,
                                           const char* image_server_url,
                                           image_ready_cb on_image_ready,
                                           void* cb_usr);
void img_client_run(struct img_client_ctx* ctx, const volatile sig_atomic_t* running);
void img_client_free(struct img_client_ctx* ctx);
