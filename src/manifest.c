#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "rovelle/blob.h"
#include "rovelle/hash.h"
#include "rovelle/manifest.h"
#include "rovelle/status.h"

#define ROVELLE_MANIFEST_HEADER "ROVELLE-MANIFEST-V1\n"
#define ROVELLE_MANIFEST_PATH_MAX 4095
#define ROVELLE_MANIFEST_LINE_SIZE \
    (64 + 1 + ROVELLE_MANIFEST_PATH_MAX + 1 + 1)

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
    char manifest_line[ROVELLE_MANIFEST_LINE_SIZE];

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

    size_t entry_count = 0;

    while (fgets(manifest_line, sizeof(manifest_line), manifest)) {
        char *newline = strchr(manifest_line, '\n');

        if (newline == NULL) {
            status = ROVELLE_INVALID_ARGUMENT;
            goto cleanup;
        }

        *newline = '\0';

        char *separator = strchr(manifest_line, '\t');

        if (separator == NULL) {
            status = ROVELLE_INVALID_ARGUMENT;
            goto cleanup;
        }

        *separator = '\0';

        const char *hash = manifest_line;
        const char *path = separator + 1;

        if (path[0] == '\0') {
            status = ROVELLE_INVALID_ARGUMENT;
            goto cleanup;
        }

        status = rovelle_blob_verify(storage_path, hash);

        if (status != ROVELLE_OK) {
            goto cleanup;
        }

        entry_count++;
    }

    if (ferror(manifest)) {
        status = ROVELLE_IO_ERROR;
        goto cleanup;
    }

    if (entry_count == 0) {
        status = ROVELLE_INVALID_ARGUMENT;
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
