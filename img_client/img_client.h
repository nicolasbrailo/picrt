#pragma once

#include <stdbool.h>
#include <stddef.h>

struct img_client_ctx;

struct img_client_ctx* img_client_init(int screen_w, int screen_h,
                                       const char* image_server_url);
void img_client_free(struct img_client_ctx* ctx);

/**
 * Returns the next image if one is available.
 * Returns true if an image was available, false otherwise.
 * data/sz point into prefetcher-owned memory, valid until the next call.
 */
bool img_client_get_image(struct img_client_ctx* ctx,
                          const unsigned char** data, size_t* sz);
