#include "prefetcher.h"

#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void* image_prefetcher_thread(void* usr);

struct image_prefetcher_ctx {
    downloader_cb downloader_impl;
    void* downloader_impl_usr;

    pthread_t thread;
    pthread_cond_t condvar;
    pthread_mutex_t cv_mut;

    size_t cache_size;
    struct prefetched_img* cache;  // ring buffer of images
    size_t cache_r;                // read index
    size_t cache_w;                // write index
    size_t prefetch_n;
};

static void cache_entry_free(struct prefetched_img* img) {
    free(img->data);
    img->data = NULL;
    img->sz = 0;
}

struct image_prefetcher_ctx* image_prefetcher_init(downloader_cb cb,
                                                   void* downloader_impl_usr,
                                                   size_t prefetch_n)
{
    if (prefetch_n == 0) {
        fprintf(stderr, "Can't use prefetcher with prefetch count = 0\n");
        return NULL;
    }

    struct image_prefetcher_ctx* ctx = malloc(sizeof(struct image_prefetcher_ctx));
    if (!ctx) {
        return NULL;
    }

    ctx->downloader_impl = cb;
    ctx->downloader_impl_usr = downloader_impl_usr;
    ctx->thread = 0;
    // +1 so the ring buffer can distinguish full from empty
    ctx->cache_size = prefetch_n + 1;
    ctx->cache_r = 0;
    ctx->cache_w = 0;
    ctx->prefetch_n = prefetch_n;

    if (pthread_cond_init(&ctx->condvar, NULL) != 0) {
        perror("pthread_cond_init");
        free(ctx);
        return NULL;
    }

    if (pthread_mutex_init(&ctx->cv_mut, NULL) != 0) {
        perror("pthread_mutex_init");
        pthread_cond_destroy(&ctx->condvar);
        free(ctx);
        return NULL;
    }

    ctx->cache = calloc(ctx->cache_size, sizeof(struct prefetched_img));
    if (!ctx->cache) {
        fprintf(stderr, "Bad alloc, can't create prefetcher\n");
        pthread_mutex_destroy(&ctx->cv_mut);
        pthread_cond_destroy(&ctx->condvar);
        free(ctx);
        return NULL;
    }

    if (pthread_create(&ctx->thread, NULL, image_prefetcher_thread, ctx) != 0) {
        perror("pthread_create");
        free(ctx->cache);
        pthread_mutex_destroy(&ctx->cv_mut);
        pthread_cond_destroy(&ctx->condvar);
        free(ctx);
        return NULL;
    }

    return ctx;
}

void image_prefetcher_free(struct image_prefetcher_ctx* ctx)
{
    if (!ctx) {
        return;
    }

    if (ctx->thread) {
        pthread_cancel(ctx->thread);
        pthread_join(ctx->thread, NULL);
    }

    pthread_cond_destroy(&ctx->condvar);
    pthread_mutex_destroy(&ctx->cv_mut);

    if (ctx->cache) {
        for (size_t i = 0; i < ctx->cache_size; ++i) {
            cache_entry_free(&ctx->cache[i]);
        }
        free(ctx->cache);
    }

    free(ctx);
}

static size_t cached_count_locked(struct image_prefetcher_ctx* ctx) {
    if (ctx->cache_w >= ctx->cache_r)
        return ctx->cache_w - ctx->cache_r;
    return ctx->cache_w + ctx->cache_size - ctx->cache_r;
}

void* image_prefetcher_thread(void* usr)
{
    struct image_prefetcher_ctx* ctx = usr;

    while (true) {
        pthread_mutex_lock(&ctx->cv_mut);
        size_t cnt = cached_count_locked(ctx);
        pthread_mutex_unlock(&ctx->cv_mut);

        while (cnt < ctx->prefetch_n) {
            pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

            size_t img_sz = 0;
            unsigned char* img_data = ctx->downloader_impl(ctx->downloader_impl_usr, &img_sz);

            pthread_mutex_lock(&ctx->cv_mut);

            size_t next_w = (ctx->cache_w + 1) % ctx->cache_size;
            if (next_w == ctx->cache_r) {
                // Buffer full, drop this image
                pthread_mutex_unlock(&ctx->cv_mut);
                free(img_data);
                pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
                break;
            }

            cache_entry_free(&ctx->cache[ctx->cache_w]);
            ctx->cache[ctx->cache_w].data = img_data;
            ctx->cache[ctx->cache_w].sz = img_sz;
            ctx->cache_w = next_w;
            cnt = cached_count_locked(ctx);

            pthread_mutex_unlock(&ctx->cv_mut);
            pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
        }

        pthread_mutex_lock(&ctx->cv_mut);
        pthread_cond_wait(&ctx->condvar, &ctx->cv_mut);
        pthread_mutex_unlock(&ctx->cv_mut);
    }

    return NULL;
}

size_t image_prefetcher_get_cached(struct image_prefetcher_ctx* ctx)
{
    pthread_mutex_lock(&ctx->cv_mut);
    size_t cnt = cached_count_locked(ctx);
    pthread_mutex_unlock(&ctx->cv_mut);
    return cnt;
}

struct prefetched_img* image_prefetcher_jump_next(struct image_prefetcher_ctx* ctx)
{
    pthread_mutex_lock(&ctx->cv_mut);

    if (ctx->cache_r == ctx->cache_w) {
        // Empty
        pthread_mutex_unlock(&ctx->cv_mut);
        return NULL;
    }

    struct prefetched_img* ret = &ctx->cache[ctx->cache_r];
    ctx->cache_r = (ctx->cache_r + 1) % ctx->cache_size;

    pthread_mutex_unlock(&ctx->cv_mut);

    // Wake prefetcher to refill
    pthread_cond_signal(&ctx->condvar);

    return ret;
}
