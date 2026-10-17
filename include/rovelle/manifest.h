#ifndef ROVELLE_MANIFEST_H
#define ROVELLE_MANIFEST_H

#include "rovelle/hash.h"
#include "rovelle/status.h"

rovelle_status_t rovelle_manifest_publish(
    const char *storage_path,
    const char *manifest_path,
    char out_revision[ROVELLE_SHA256_HEX_SIZE]
);

#endif
