#ifndef ROVELLE_HASH_H
#define ROVELLE_HASH_H

#include "rovelle/status.h"

#define ROVELLE_SHA256_HEX_LENGTH 64
#define ROVELLE_SHA256_HEX_SIZE (ROVELLE_SHA256_HEX_LENGTH + 1)

rovelle_status_t rovelle_hash_file(
    const char *path,
    char out_hex[ROVELLE_SHA256_HEX_SIZE]
);

int rovelle_hash_is_valid(const char *hash);

#endif
