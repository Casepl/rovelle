#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "rovelle/blob.h"
#include "rovelle/manifest.h"
#include "rovelle/status.h"

#define ROVELLE_MANIFEST_HEADER "ROVELLE-MANIFEST-V1\n"

rovelle_status_t rovelle_manifest_publish(
    const char *storage_path,
    const char *manifest_path,
    char out_revision[ROVELLE_SHA256_HEX_SIZE]
)
{
    if (storage_path == NULL ||
        manifest_path == NULL ||
        out_revision == NULL) {
        return ROVELLE_INVALID_ARGUMENT;
    }

    rovelle_status_t status = ROVELLE_OK;
    char header[sizeof(ROVELLE_MANIFEST_HEADER)];
    FILE *manifest = fopen(manifest_path, "rb");

    if (manifest == NULL) {
        status = errno == ENOENT
            ? ROVELLE_NOT_FOUND
            : ROVELLE_IO_ERROR;

        goto cleanup;
    }

    if (fgets(header, sizeof(ROVELLE_MANIFEST_HEADER), manifest) == NULL) {
        status = ferror(manifest)
            ? ROVELLE_IO_ERROR
            : ROVELLE_INVALID_ARGUMENT;

        goto cleanup;
    }

    if (strcmp(ROVELLE_MANIFEST_HEADER, header) != 0) {
        status = ROVELLE_INVALID_ARGUMENT;

        goto cleanup;
    }

    int ch = fgetc(manifest);

    if (ch == EOF) {
        status = ferror(manifest)
            ? ROVELLE_IO_ERROR
            : ROVELLE_INVALID_ARGUMENT;

        goto cleanup;
    }

    if (fclose(manifest) != 0) {
        manifest = NULL;
        status = ROVELLE_IO_ERROR;
        goto cleanup;
    }

    manifest = NULL;

    status = rovelle_blob_put(storage_path, manifest_path, out_revision);

cleanup:
    if (manifest != NULL) {
        if (fclose(manifest) != 0 &&
            status == ROVELLE_OK) {
            status = ROVELLE_IO_ERROR;
        }
    }

    return status;
}
