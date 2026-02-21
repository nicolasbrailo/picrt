#pragma once

#include <stdbool.h>
#include <stddef.h>

struct downloader_ctx;

struct downloader_ctx* downloader_init(const char* www_url);
void downloader_free(struct downloader_ctx* ctx);

// Downloads an image to memory. Returns the buffer and sets *out_sz.
// Caller takes ownership of the returned buffer (must free it).
// Returns NULL on failure.
unsigned char* downloader_get_one(struct downloader_ctx* ctx, size_t* out_sz);
