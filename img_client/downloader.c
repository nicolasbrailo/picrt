#include "downloader.h"

#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct downloader_ctx {
    char* www_url;
    CURL* curl_handle;
    unsigned char* mem_buf;
    size_t mem_buf_sz;
};

static size_t write_data(void* ptr, size_t size, size_t nmemb, void* usr);

struct downloader_ctx* downloader_init(const char* www_url)
{
    if (!www_url) {
        fprintf(stderr, "Missing www_url\n");
        return NULL;
    }

    struct downloader_ctx* ctx = malloc(sizeof(struct downloader_ctx));
    if (!ctx) {
        fprintf(stderr, "bad alloc\n");
        return NULL;
    }

    ctx->mem_buf = NULL;
    ctx->mem_buf_sz = 0;
    ctx->curl_handle = NULL;

    ctx->www_url = strdup(www_url);
    if (!ctx->www_url) {
        fprintf(stderr, "bad alloc\n");
        free(ctx);
        return NULL;
    }

    {
        const int ret = curl_global_init(CURL_GLOBAL_ALL);
        if (ret != 0) {
            fprintf(stderr, "Failed to global init curl: %s\n",
                    curl_easy_strerror(ret));
            downloader_free(ctx);
            return NULL;
        }
    }

    ctx->curl_handle = curl_easy_init();
    if (!ctx->curl_handle) {
        fprintf(stderr, "Failed to create curl_handle\n");
        downloader_free(ctx);
        return NULL;
    }

    int ret = CURLE_OK;
    ret = ret | curl_easy_setopt(ctx->curl_handle, CURLOPT_VERBOSE, 0L);
    ret = ret | curl_easy_setopt(ctx->curl_handle, CURLOPT_NOPROGRESS, 1L);
    ret = ret | curl_easy_setopt(ctx->curl_handle, CURLOPT_WRITEFUNCTION, write_data);
    ret = ret | curl_easy_setopt(ctx->curl_handle, CURLOPT_WRITEDATA, ctx);
    ret = ret | curl_easy_setopt(ctx->curl_handle, CURLOPT_URL, ctx->www_url);
    if (ret != CURLE_OK) {
        fprintf(stderr, "Failed to setup curl: %s\n", curl_easy_strerror(ret));
        downloader_free(ctx);
        return NULL;
    }

    return ctx;
}

void downloader_free(struct downloader_ctx* ctx)
{
    if (!ctx) {
        return;
    }

    if (ctx->curl_handle) {
        curl_easy_cleanup(ctx->curl_handle);
    }

    curl_global_cleanup();

    free(ctx->mem_buf);
    free(ctx->www_url);
    free(ctx);
}

static size_t write_data(void* ptr, size_t size, size_t nmemb, void* usr)
{
    size_t chunk_sz = size * nmemb;
    struct downloader_ctx* ctx = usr;

    unsigned char* reallocd = realloc(ctx->mem_buf, ctx->mem_buf_sz + chunk_sz);
    if (!reallocd) {
        fprintf(stderr, "Fail to download, bad alloc\n");
        return 0;
    }

    ctx->mem_buf = reallocd;
    memcpy(ctx->mem_buf + ctx->mem_buf_sz, ptr, chunk_sz);
    ctx->mem_buf_sz += chunk_sz;

    return chunk_sz;
}

unsigned char* downloader_get_one(struct downloader_ctx* ctx, size_t* out_sz)
{
    ctx->mem_buf = NULL;
    ctx->mem_buf_sz = 0;

    const CURLcode ret = curl_easy_perform(ctx->curl_handle);
    if (ret != CURLE_OK) {
        fprintf(stderr, "Fail to download from %s: %s\n", ctx->www_url,
               curl_easy_strerror(ret));
        free(ctx->mem_buf);
        ctx->mem_buf = NULL;
        ctx->mem_buf_sz = 0;
        *out_sz = 0;
        return NULL;
    }

    unsigned char* buf = ctx->mem_buf;
    *out_sz = ctx->mem_buf_sz;

    // Detach from ctx so caller owns it
    ctx->mem_buf = NULL;
    ctx->mem_buf_sz = 0;

    return buf;
}
