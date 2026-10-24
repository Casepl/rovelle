#ifndef ROVELLE_BLOB_H
#define ROVELLE_BLOB_H

#include "rovelle/hash.h"
#include "rovelle/status.h"

rovelle_status_t rovelle_blob_put(
    const char *store_path,
    const char *source_path,
    char out_hash[ROVELLE_SHA256_HEX_SIZE]
);

rovelle_status_t rovelle_blob_get(
    const char *store_path,
    const char hash[ROVELLE_SHA256_HEX_SIZE],
    const char *destination_path
);

rovelle_status_t rovelle_blob_verify(
    const char *store_path,
    const char hash[ROVELLE_SHA256_HEX_SIZE]
);

#endif
