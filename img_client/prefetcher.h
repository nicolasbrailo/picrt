#pragma once

#include <stdbool.h>
#include <stddef.h>

struct image_prefetcher_ctx;

/**
 * An image buffer downloaded by the prefetcher.
 * Owned by the prefetcher — do not free.
 */
struct prefetched_img {
    unsigned char* data;
    size_t sz;
};

/**
 * Callback to fetch an image in-memory. Caller takes ownership of the buffer.
 * Should set *out_sz and return a malloc'd buffer, or NULL on failure.
 */
typedef unsigned char* (*downloader_cb)(void* usr, size_t* out_sz);

/**
 * Creates a prefetcher. Will prefetch up to prefetch_n images ahead
 * using the provided callback in a background thread.
 */
struct image_prefetcher_ctx* image_prefetcher_init(downloader_cb cb,
                                                   void* downloader_impl_usr,
                                                   size_t prefetch_n);

void image_prefetcher_free(struct image_prefetcher_ctx* ctx);

/**
 * Returns the number of currently cached and available images.
 */
size_t image_prefetcher_get_cached(struct image_prefetcher_ctx* ctx);

/**
 * Return the next image in the cache, or NULL if none available.
 * The returned pointer is owned by the prefetcher — valid until the
 * next call to jump_next.
 */
struct prefetched_img* image_prefetcher_jump_next(struct image_prefetcher_ctx* ctx);
