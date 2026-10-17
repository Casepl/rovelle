#include <openssl/evp.h>
#include <stdio.h>

#include "rovelle/hash.h"

rovelle_status_t rovelle_hash_file(
    const char *path,
    char out_hex[ROVELLE_SHA256_HEX_SIZE]
)
{
    rovelle_status_t status = ROVELLE_OK;

    if (path == NULL || out_hex == NULL) {
        return ROVELLE_INVALID_ARGUMENT;
    }

    static const char hex[] = "0123456789abcdef";

    EVP_MD_CTX *ctx = NULL;
    EVP_MD *sha256 = NULL;
    unsigned char chunk[4096];
    unsigned int len = 0;
    unsigned char *digest = NULL;
    size_t items_read;

    FILE *file = fopen(path, "rb");

    if (file == NULL) {
        return ROVELLE_NOT_FOUND;
    }

    if ((ctx = EVP_MD_CTX_new()) == NULL) {
        status = ROVELLE_IO_ERROR;
        goto cleanup;
    }

    sha256 = EVP_MD_fetch(NULL, "SHA256", NULL);

    if (sha256 == NULL) {
        status = ROVELLE_IO_ERROR;
        goto cleanup;
    }

    if (EVP_DigestInit_ex(ctx, sha256, NULL) != 1) {
        status = ROVELLE_IO_ERROR;
        goto cleanup;
    }

    while ((items_read = fread(chunk, 1, sizeof(chunk), file)) > 0) {
        if (EVP_DigestUpdate(ctx, chunk, items_read) != 1) {
            status = ROVELLE_IO_ERROR;
            goto cleanup;
        }
    }

    if (ferror(file)) {
        status = ROVELLE_IO_ERROR;
        goto cleanup;
    }

    int digest_size = EVP_MD_get_size(sha256);

    if (digest_size <= 0) {
        status = ROVELLE_IO_ERROR;
        goto cleanup;
    }

    digest = OPENSSL_malloc((size_t)digest_size);

    if (digest == NULL) {
        status = ROVELLE_IO_ERROR;
        goto cleanup;
    }

    if (EVP_DigestFinal_ex(ctx, digest, &len) != 1) {
        status = ROVELLE_IO_ERROR;
        goto cleanup;
    }

    for (unsigned int i = 0; i < len; ++i) {
        out_hex[i * 2] = hex[digest[i] >> 4];
        out_hex[i * 2 + 1] = hex[digest[i] & 0x0f];
    }

    out_hex[len * 2] = '\0';

cleanup:
    OPENSSL_free(digest);
    EVP_MD_CTX_free(ctx);
    EVP_MD_free(sha256);

    if (file != NULL) {
        fclose(file);
    }

    return status;
}
