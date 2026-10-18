#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "rovelle/blob.h"
#include "rovelle/hash.h"

#define ROVELLE_PATH_SIZE 4096

static rovelle_status_t ensure_directory(const char *path)
{
    struct stat info;

    if (path == NULL) {
        return ROVELLE_INVALID_ARGUMENT;
    }

    if (stat(path, &info) == 0) {
        if (S_ISDIR(info.st_mode)) {
            return ROVELLE_OK;
        }

        return ROVELLE_IO_ERROR;
    }

    if (errno != ENOENT) {
        return ROVELLE_IO_ERROR;
    }

    if (mkdir(path, 0755) != 0) {
        return ROVELLE_IO_ERROR;
    }

    return ROVELLE_OK;
}

static rovelle_status_t copy_stream(FILE *source, FILE *destination)
{
    unsigned char buffer[64 * 1024];
    size_t items_read;

    if (source == NULL || destination == NULL) {
        return ROVELLE_INVALID_ARGUMENT;
    }

    while ((items_read = fread(buffer, 1, sizeof(buffer), source)) > 0) {
        size_t items_written =
            fwrite(buffer, 1, items_read, destination);

        if (items_written != items_read) {
            return ROVELLE_IO_ERROR;
        }
    }

    if (ferror(source) != 0) {
        return ROVELLE_IO_ERROR;
    }

    return ROVELLE_OK;
}

static rovelle_status_t format_blob_path(
    const char *store_path,
    const char hash[ROVELLE_SHA256_HEX_SIZE],
    char *out_path,
    size_t out_path_size
)
{
    if (store_path == NULL ||
        hash == NULL ||
        out_path == NULL ||
        out_path_size == 0) {
        return ROVELLE_INVALID_ARGUMENT;
    }

    int written = snprintf(
        out_path,
        out_path_size,
        "%s/objects/%.2s/%s",
        store_path,
        hash,
        hash + 2
    );

    if (written < 0 || (size_t)written >= out_path_size) {
        return ROVELLE_IO_ERROR;
    }

    return ROVELLE_OK;
}

rovelle_status_t rovelle_blob_put(
    const char *store_path,
    const char *source_path,
    char out_hash[ROVELLE_SHA256_HEX_SIZE]
)
{
    char objects_path[ROVELLE_PATH_SIZE];
    char tmp_path[ROVELLE_PATH_SIZE];
    char temporary_path[ROVELLE_PATH_SIZE];
    char shard_path[ROVELLE_PATH_SIZE];
    char final_path[ROVELLE_PATH_SIZE];

    FILE *source_file = NULL;
    FILE *temporary = NULL;

    int temporary_fd = -1;
    int temporary_exists = 0;
    int written;

    struct stat file_info;

    rovelle_status_t status = ROVELLE_OK;

    if (store_path == NULL ||
        source_path == NULL ||
        out_hash == NULL) {
        return ROVELLE_INVALID_ARGUMENT;
    }

    status = ensure_directory(store_path);

    if (status != ROVELLE_OK) {
        return status;
    }

    written = snprintf(
        objects_path,
        sizeof(objects_path),
        "%s/objects",
        store_path
    );

    if (written < 0 ||
        (size_t)written >= sizeof(objects_path)) {
        return ROVELLE_IO_ERROR;
    }

    status = ensure_directory(objects_path);

    if (status != ROVELLE_OK) {
        return status;
    }

    written = snprintf(
        tmp_path,
        sizeof(tmp_path),
        "%s/tmp",
        store_path
    );

    if (written < 0 ||
        (size_t)written >= sizeof(tmp_path)) {
        return ROVELLE_IO_ERROR;
    }

    status = ensure_directory(tmp_path);

    if (status != ROVELLE_OK) {
        return status;
    }

    written = snprintf(
        temporary_path,
        sizeof(temporary_path),
        "%s/blob-XXXXXX",
        tmp_path
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

    temporary = fdopen(temporary_fd, "wb");

    if (temporary == NULL) {
        status = ROVELLE_IO_ERROR;
        goto cleanup;
    }

    temporary_fd = -1;

    source_file = fopen(source_path, "rb");

    if (source_file == NULL) {
        status = errno == ENOENT
            ? ROVELLE_NOT_FOUND
            : ROVELLE_IO_ERROR;

        goto cleanup;
    }

    status = copy_stream(source_file, temporary);

    if (status != ROVELLE_OK) {
        goto cleanup;
    }

    if (fclose(source_file) != 0) {
        source_file = NULL;
        status = ROVELLE_IO_ERROR;
        goto cleanup;
    }

    source_file = NULL;

    if (fflush(temporary) != 0) {
        status = ROVELLE_IO_ERROR;
        goto cleanup;
    }

    int temporary_file_number = fileno(temporary);

    if (temporary_file_number == -1) {
        status = ROVELLE_IO_ERROR;
        goto cleanup;
    }

    if (fsync(temporary_file_number) != 0) {
        status = ROVELLE_IO_ERROR;
        goto cleanup;
    }

    if (fclose(temporary) != 0) {
        temporary = NULL;
        status = ROVELLE_IO_ERROR;
        goto cleanup;
    }

    temporary = NULL;

    status = rovelle_hash_file(temporary_path, out_hash);

    if (status != ROVELLE_OK) {
        goto cleanup;
    }

    written = snprintf(
        shard_path,
        sizeof(shard_path),
        "%s/objects/%.2s",
        store_path,
        out_hash
    );

    if (written < 0 ||
        (size_t)written >= sizeof(shard_path)) {
        status = ROVELLE_IO_ERROR;
        goto cleanup;
    }

    status = ensure_directory(shard_path);

    if (status != ROVELLE_OK) {
        goto cleanup;
    }

    status = format_blob_path(
        store_path,
        out_hash,
        final_path,
        sizeof(final_path)
    );

    if (status != ROVELLE_OK) {
        goto cleanup;
    }

    if (stat(final_path, &file_info) == 0) {
        if (S_ISREG(file_info.st_mode)) {
            status = ROVELLE_OK;
        } else {
            status = ROVELLE_CONFLICT;
        }

        goto cleanup;
    }

    if (errno != ENOENT) {
        status = ROVELLE_IO_ERROR;
        goto cleanup;
    }

    if (rename(temporary_path, final_path) != 0) {
        status = ROVELLE_IO_ERROR;
        goto cleanup;
    }

    temporary_exists = 0;
    status = ROVELLE_OK;

cleanup:
    if (source_file != NULL) {
        if (fclose(source_file) != 0 &&
            status == ROVELLE_OK) {
            status = ROVELLE_IO_ERROR;
        }
    }

    if (temporary != NULL) {
        if (fclose(temporary) != 0 &&
            status == ROVELLE_OK) {
            status = ROVELLE_IO_ERROR;
        }
    } else if (temporary_fd != -1) {
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

rovelle_status_t rovelle_blob_get(
    const char *store_path,
    const char hash[ROVELLE_SHA256_HEX_SIZE],
    const char *destination_path
)
{
    char blob_path[ROVELLE_PATH_SIZE];
    char temporary_path[ROVELLE_PATH_SIZE];
    char actual_hash[ROVELLE_SHA256_HEX_SIZE];

    FILE *source = NULL;
    FILE *temporary = NULL;

    int temporary_fd = -1;
    int temporary_exists = 0;
    int written;

    rovelle_status_t status = ROVELLE_OK;

    if (store_path == NULL ||
        destination_path == NULL ||
        rovelle_hash_is_valid(hash) == 0) {
        return ROVELLE_INVALID_ARGUMENT;
    }

    status = format_blob_path(store_path, hash, blob_path, sizeof(blob_path));

    if (status != ROVELLE_OK) {
        return status;
    }

    source = fopen(blob_path, "rb");

    if (source == NULL) {
        status = errno == ENOENT ? ROVELLE_NOT_FOUND : ROVELLE_IO_ERROR;

        goto cleanup;
    }

    written = snprintf(
        temporary_path,
        sizeof(temporary_path),
        "%s.tmp-XXXXXX",
        destination_path
    );

    if (written < 0 ||
        (size_t)written >= sizeof(temporary_path)) {
        status = ROVELLE_IO_ERROR;
        goto cleanup;
    }

    temporary_fd = mkstemp(temporary_path);

    if (temporary_fd == -1) {
        status = ROVELLE_IO_ERROR;
        goto cleanup;
    }

    temporary_exists = 1;

    temporary = fdopen(temporary_fd, "wb");

    if (temporary == NULL) {
        status = ROVELLE_IO_ERROR;
        goto cleanup;
    }

    temporary_fd = -1;

    status = copy_stream(source, temporary);

    if (status != ROVELLE_OK) {
        goto cleanup;
    }

    if (fclose(source) != 0) {
        source = NULL;
        status = ROVELLE_IO_ERROR;
        goto cleanup;
    }

    source = NULL;

    if (fflush(temporary) != 0) {
        status = ROVELLE_IO_ERROR;
        goto cleanup;
    }

    int temporary_file_number = fileno(temporary);

    if (temporary_file_number == -1) {
        status = ROVELLE_IO_ERROR;
        goto cleanup;
    }

    if (fsync(temporary_file_number) != 0) {
        status = ROVELLE_IO_ERROR;
        goto cleanup;
    }

    if (fclose(temporary) != 0) {
        temporary = NULL;
        status = ROVELLE_IO_ERROR;
        goto cleanup;
    }

    temporary = NULL;

    status = rovelle_hash_file(temporary_path, actual_hash);

    if (status != ROVELLE_OK) {
        goto cleanup;
    }

    int hash_comparison = strcmp(actual_hash, hash);

    if (hash_comparison != 0) {
        status = ROVELLE_CORRUPT;
        goto cleanup;
    }

    if (rename(temporary_path, destination_path) != 0) {
        status = ROVELLE_IO_ERROR;
        goto cleanup;
    }

    temporary_exists = 0;
    status = ROVELLE_OK;

cleanup:
    if (source != NULL) {
        if (fclose(source) != 0 &&
            status == ROVELLE_OK) {
            status = ROVELLE_IO_ERROR;
        }
    }

    if (temporary != NULL) {
        if (fclose(temporary) != 0 &&
            status == ROVELLE_OK) {
            status = ROVELLE_IO_ERROR;
        }
    } else if (temporary_fd != -1) {
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
