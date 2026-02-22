#include "img_client.h"
#include "downloader.h"
#include "prefetcher.h"

#include <curl/curl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PREFETCH_N 3
#define MAX_URL_LEN 512

struct img_client_ctx {
    struct downloader_ctx* dl;
    struct image_prefetcher_ctx* prefetcher;
};

static unsigned char* dl_callback(void* usr, size_t* out_sz) {
    struct downloader_ctx* dl = usr;
    return downloader_get_one(dl, out_sz);
}

struct curl_buf {
    char* data;
    size_t sz;
};

static size_t curl_buf_write(void* ptr, size_t size, size_t nmemb, void* usr) {
    struct curl_buf* buf = usr;
    size_t chunk = size * nmemb;
    char* tmp = realloc(buf->data, buf->sz + chunk + 1);
    if (!tmp) return 0;
    buf->data = tmp;
    memcpy(buf->data + buf->sz, ptr, chunk);
    buf->sz += chunk;
    buf->data[buf->sz] = '\0';
    return chunk;
}

// Simple HTTP GET that returns the response body as a null-terminated string.
// Caller must free the result.
static char* http_get(const char* url) {
    CURL* curl = curl_easy_init();
    if (!curl) return NULL;

    struct curl_buf buf = {NULL, 0};
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_buf_write);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);

    CURLcode ret = curl_easy_perform(curl);
    if (ret != CURLE_OK) {
        fprintf(stderr, "HTTP GET %s failed: %s\n", url, curl_easy_strerror(ret));
        curl_easy_cleanup(curl);
        free(buf.data);
        return NULL;
    }

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_easy_cleanup(curl);

    if (http_code != 200) {
        fprintf(stderr, "HTTP GET %s returned %ld\n", url, http_code);
        free(buf.data);
        return NULL;
    }

    return buf.data;
}

static bool register_client(const char* base_url, int screen_w, int screen_h, char* img_url, size_t img_url_sz) {
    char url[MAX_URL_LEN];

    // Register and get client id
    snprintf(url, MAX_URL_LEN, "%s/client_register", base_url);
    char* client_id = http_get(url);
    if (!client_id) {
        fprintf(stderr, "Failed to register with image server\n");
        return false;
    }
    // Trim trailing whitespace/newlines
    size_t len = strlen(client_id);
    while (len > 0 && (client_id[len-1] == '\n' || client_id[len-1] == '\r' || client_id[len-1] == ' '))
        client_id[--len] = '\0';

    printf("Registered with image server as client %s\n", client_id);

    // Configure target size
    snprintf(url, MAX_URL_LEN, "%s/client_cfg/%s/target_size/%dx%d", base_url, client_id, screen_w, screen_h);
    char* resp = http_get(url);
    if (resp) {
        printf("Set target size: %s\n", resp);
        free(resp);
    } else {
        fprintf(stderr, "Failed to set image server target screen size\n");
    }

    // Disable QR code
    snprintf(url, MAX_URL_LEN, "%s/client_cfg/%s/embed_info_qr_code/false", base_url, client_id);
    resp = http_get(url);
    if (resp) {
        printf("Disabled QR code: %s\n", resp);
        free(resp);
    } else {
        fprintf(stderr, "Failed to disable image qr code in image server\n");
    }

    // Build the image fetch URL
    snprintf(img_url, img_url_sz, "%s/get_next_img/%s", base_url, client_id);
    free(client_id);
    return true;
}

struct img_client_ctx* img_client_init(int screen_w, int screen_h,
                                       const char* image_server_url) {
    struct img_client_ctx* ctx = malloc(sizeof(struct img_client_ctx));
    if (!ctx) {
        return NULL;
    }

    char img_url[MAX_URL_LEN];
    if (!register_client(image_server_url, screen_w, screen_h, img_url, MAX_URL_LEN)) {
        free(ctx);
        return NULL;
    }

    printf("Registered with image server, will fetch from '%s'\n", img_url);

    ctx->dl = downloader_init(img_url);
    if (!ctx->dl) {
        free(ctx);
        return NULL;
    }

    ctx->prefetcher = image_prefetcher_init(dl_callback, ctx->dl, PREFETCH_N);
    if (!ctx->prefetcher) {
        downloader_free(ctx->dl);
        free(ctx);
        return NULL;
    }

    return ctx;
}

void img_client_free(struct img_client_ctx* ctx) {
    if (!ctx) {
        return;
    }
    image_prefetcher_free(ctx->prefetcher);
    downloader_free(ctx->dl);
    free(ctx);
}

bool img_client_get_image(struct img_client_ctx* ctx,
                          const unsigned char** data, size_t* sz) {
    struct prefetched_img* img = image_prefetcher_jump_next(ctx->prefetcher);
    if (!img || !img->data) {
        return false;
    }

    *data = img->data;
    *sz = img->sz;
    return true;
}
