#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "rovelle/blob.h"
#include "rovelle/hash.h"
#include "rovelle/manifest.h"
#include "rovelle/status.h"

#define ROVELLE_MANIFEST_HEADER "ROVELLE-MANIFEST-V1\n"
#define ROVELLE_MANIFEST_PATH_MAX 4095
#define ROVELLE_MANIFEST_LINE_SIZE \
    (64 + 1 + ROVELLE_MANIFEST_PATH_MAX + 1 + 1)

typedef struct {
    char *hash;
    char *logical_path;
} manifest_entry_t;

static rovelle_status_t create_parent_directories(
    const char *destination_path,
    const char *logical_path
)
{
    struct stat info;
    char full_path[ROVELLE_MANIFEST_PATH_MAX + 1];

    if (logical_path == NULL || destination_path == NULL) {
        return ROVELLE_INVALID_ARGUMENT;
    }

    int written = snprintf(
        full_path,
        sizeof(full_path),
        "%s/%s",
        destination_path,
        logical_path
    );

    if (written < 0 || (size_t)written >= sizeof(full_path)) {
        return ROVELLE_IO_ERROR;
    }

    for (size_t i = strlen(destination_path) + 1;
         full_path[i] != '\0';
         i++) {
        if (full_path[i] == '/') {
            full_path[i] = '\0';

            if (mkdir(full_path, 0755) != 0 && errno != EEXIST) {
                return ROVELLE_IO_ERROR;
            }

            if (lstat(full_path, &info) != 0) {
                return ROVELLE_IO_ERROR;
            }

            if (!S_ISDIR(info.st_mode)) {
                return ROVELLE_CONFLICT;
            }

            full_path[i] = '/';
        }
    }

    return ROVELLE_OK;
}

static int logical_path_is_safe(const char *path)
{
    if (path == NULL || path[0] == '\0') {
        return 0;
    }

    size_t segment_start = 0;
    for (size_t i = 0; ; i++) {
        if (path[i] == '\\' || path[i] == '\t') {
            return 0;
        }

        if (path[i] == '/' || path[i] == '\0') {
            size_t length = i - segment_start;

            if (length == 0 ||
                (length == 1 && path[segment_start] == '.') ||
                (length == 2 &&
                 path[segment_start] == '.' &&
                 path[segment_start + 1] == '.')) {
                return 0;
            }

            if (path[i] == '\0') {
                return 1;
            }

            segment_start = i + 1;
        }
    }
}

static rovelle_status_t create_restore_directory(const char *path)
{
    if (path == NULL || path[0] == '\0') {
        return ROVELLE_INVALID_ARGUMENT;
    }

    if (mkdir(path, 0755) == 0) {
        return ROVELLE_OK;
    }

    return errno == EEXIST ? ROVELLE_CONFLICT : ROVELLE_IO_ERROR;
}

static rovelle_status_t validate_manifest_header(FILE *manifest)
{
    char header[sizeof(ROVELLE_MANIFEST_HEADER)];

    if (manifest == NULL) {
        return ROVELLE_INVALID_ARGUMENT;
    }

    if (fgets(header, sizeof(header), manifest) == NULL) {
        return ferror(manifest)
            ? ROVELLE_IO_ERROR
            : ROVELLE_INVALID_ARGUMENT;
    }

    return strcmp(ROVELLE_MANIFEST_HEADER, header) == 0
        ? ROVELLE_OK
        : ROVELLE_INVALID_ARGUMENT;
}

static rovelle_status_t parse_manifest_line(
    char *manifest_line,
    manifest_entry_t *out_entry
)
{
    if (manifest_line == NULL || out_entry == NULL) {
        return ROVELLE_INVALID_ARGUMENT;
    }

    char *newline = strchr(manifest_line, '\n');

    if (newline == NULL) {
        return ROVELLE_INVALID_ARGUMENT;
    }

    *newline = '\0';

    char *separator = strchr(manifest_line, '\t');

    if (separator == NULL) {
        return ROVELLE_INVALID_ARGUMENT;
    }

    *separator = '\0';

    if (rovelle_hash_is_valid(manifest_line) == 0 ||
        separator[1] == '\0') {
        return ROVELLE_INVALID_ARGUMENT;
    }

    out_entry->hash = manifest_line;
    out_entry->logical_path = separator + 1;

    return ROVELLE_OK;
}

static rovelle_status_t restore_manifest_entry(
    const char *store_path,
    const char *destination_path,
    const manifest_entry_t *entry
)
{
    char restored_file_path[ROVELLE_MANIFEST_PATH_MAX + 1];

    if (store_path == NULL ||
        destination_path == NULL ||
        entry == NULL) {
        return ROVELLE_INVALID_ARGUMENT;
    }

    if (logical_path_is_safe(entry->logical_path) == 0) {
        return ROVELLE_INVALID_ARGUMENT;
    }

    rovelle_status_t status = create_parent_directories(
        destination_path,
        entry->logical_path
    );

    if (status != ROVELLE_OK) {
        return status;
    }

    int written = snprintf(
        restored_file_path,
        sizeof(restored_file_path),
        "%s/%s",
        destination_path,
        entry->logical_path
    );

    if (written < 0 ||
        (size_t)written >= sizeof(restored_file_path)) {
        return ROVELLE_IO_ERROR;
    }

    return rovelle_blob_get(
        store_path,
        entry->hash,
        restored_file_path
    );
}

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
    char manifest_line[ROVELLE_MANIFEST_LINE_SIZE];

    FILE *manifest = fopen(manifest_path, "rb");

    if (manifest == NULL) {
        return errno == ENOENT
            ? ROVELLE_NOT_FOUND
            : ROVELLE_IO_ERROR;
    }

    status = validate_manifest_header(manifest);

    if (status != ROVELLE_OK) {
        goto cleanup;
    }

    size_t entry_count = 0;

    while (fgets(manifest_line, sizeof(manifest_line), manifest)) {
        manifest_entry_t entry;

        status = parse_manifest_line(manifest_line, &entry);

        if (status != ROVELLE_OK) {
            goto cleanup;
        }

        status = rovelle_blob_verify(storage_path, entry.hash);

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


rovelle_status_t rovelle_manifest_restore(
    const char *store_path,
    const char revision[ROVELLE_SHA256_HEX_SIZE],
    const char *destination_path
)
{
    FILE *manifest = NULL;
    char temporary_path[ROVELLE_MANIFEST_PATH_MAX];
    char manifest_line[ROVELLE_MANIFEST_LINE_SIZE];

    int temporary_fd = -1;
    int temporary_exists = 0;

    if (store_path == NULL ||
        revision == NULL ||
        destination_path == NULL) {
        return ROVELLE_INVALID_ARGUMENT;
    }

    if (rovelle_hash_is_valid(revision) == 0) {
        return ROVELLE_INVALID_ARGUMENT;
    }

    rovelle_status_t status = create_restore_directory(destination_path);

    if (status != ROVELLE_OK) {
        return status;
    }

    int written = snprintf(
        temporary_path,
        sizeof(temporary_path),
        "%s.manifest-XXXXXX",
        destination_path
    );

    if (written < 0 ||
        (size_t)written >= sizeof(temporary_path)) {
        return ROVELLE_IO_ERROR;
    }

    temporary_fd = mkstemp(temporary_path);

    if (temporary_fd == -1) {
        return ROVELLE_IO_ERROR;
    }

    temporary_exists = 1;

    int close_result = close(temporary_fd);
    temporary_fd = -1;

    if (close_result != 0) {
        status = ROVELLE_IO_ERROR;
        goto cleanup;
    }

    status = rovelle_blob_get(store_path, revision, temporary_path);

    if (status != ROVELLE_OK) {
        goto cleanup;
    }

    manifest = fopen(temporary_path, "rb");

    if (manifest == NULL) {
        status = errno == ENOENT
            ? ROVELLE_NOT_FOUND
            : ROVELLE_IO_ERROR;

        goto cleanup;
    }

    status = validate_manifest_header(manifest);

    if (status != ROVELLE_OK) {
        goto cleanup;
    }

    size_t entry_count = 0;

    while (fgets(manifest_line, sizeof(manifest_line), manifest)) {
        manifest_entry_t entry;

        status = parse_manifest_line(manifest_line, &entry);

        if (status != ROVELLE_OK) {
            goto cleanup;
        }

        status = restore_manifest_entry(
            store_path,
            destination_path,
            &entry
        );

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

cleanup:
    if (manifest != NULL) {
        if (fclose(manifest) != 0 && status == ROVELLE_OK) {
            status = ROVELLE_IO_ERROR;
        }
    }

    if (temporary_fd != -1) {
        if (close(temporary_fd) != 0 &&
            status == ROVELLE_OK) {
            status = ROVELLE_IO_ERROR;
        }
    }

    if (temporary_exists != 0) {
        if (unlink(temporary_path) != 0 &&
            status == ROVELLE_OK) {
            status = ROVELLE_IO_ERROR;
        }
    }

    return status;
}
