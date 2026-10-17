#include <stdio.h>
#include <string.h>

#include "rovelle/blob.h"
#include "rovelle/hash.h"
#include "rovelle/manifest.h"
#include "rovelle/status.h"
#include "rovelle/version.h"

static void print_help(FILE *stream)
{
    fputs("Usage:\n", stream);
    fputs("  rovelle help\n", stream);
    fputs("  rovelle version\n", stream);
    fputs("  rovelle hash <file>\n", stream);
    fputs("  rovelle blob put <store> <file>\n", stream);
    fputs("  rovelle blob get <store> <hash> <destination>\n", stream);
    fputs(
        "  rovelle manifest publish <store> <manifest-file>\n\n",
        stream
    );

    fputs("Commands:\n", stream);
    fputs(
        "  help                                      Show this help\n",
        stream
    );
    fputs(
        "  version                                   Print the project version\n",
        stream
    );
    fputs(
        "  hash <file>                               Print the SHA-256 of a file\n",
        stream
    );
    fputs(
        "  blob put <store> <file>                   Store a file by its SHA-256\n",
        stream
    );
    fputs(
        "  blob get <store> <hash> <destination>     Restore and verify a blob\n",
        stream
    );
    fputs(
        "  manifest publish <store> <manifest-file>  Publish manifest\n",
        stream
    );
}

static int is_command_help(int argc, char **argv)
{
    if (strcmp(argv[argc - 1], "--help") != 0) {
        return 0;
    }

    if (argc == 3) {
        return strcmp(argv[1], "version") == 0 ||
               strcmp(argv[1], "hash") == 0;
    }

    if (argc == 4 && strcmp(argv[1], "blob") == 0) {
        return strcmp(argv[2], "put") == 0 ||
               strcmp(argv[2], "get") == 0;
    }

    return argc == 4 &&
           strcmp(argv[1], "manifest") == 0 &&
           strcmp(argv[2], "publish") == 0;
}

int main(int argc, char **argv)
{
    if (argc == 1) {
        print_help(stdout);
        return 0;
    }

    if (is_command_help(argc, argv)) {
        print_help(stdout);
        return 0;
    }

    if (argc == 2 &&
        (strcmp(argv[1], "help") == 0 ||
         strcmp(argv[1], "--help") == 0)) {
        print_help(stdout);
        return 0;
    }

    if (argc == 2 &&
        (strcmp(argv[1], "version") == 0 ||
         strcmp(argv[1], "--version") == 0)) {
        printf("rovelle %s\n", rovelle_version());
        return 0;
    }

    if (argc == 3 && strcmp(argv[1], "hash") == 0) {
        char hash[ROVELLE_SHA256_HEX_SIZE];

        rovelle_status_t status =
            rovelle_hash_file(argv[2], hash);

        if (status != ROVELLE_OK) {
            fprintf(
                stderr,
                "hash failed: %s\n",
                rovelle_status_name(status)
            );

            return 1;
        }

        printf("%s\n", hash);
        return 0;
    }

    if (argc == 5 &&
        strcmp(argv[1], "blob") == 0 &&
        strcmp(argv[2], "put") == 0) {
        const char *store_path = argv[3];
        const char *source_path = argv[4];

        char hash[ROVELLE_SHA256_HEX_SIZE];

        rovelle_status_t status = rovelle_blob_put(
            store_path,
            source_path,
            hash
        );

        if (status != ROVELLE_OK) {
            fprintf(
                stderr,
                "blob put failed: %s\n",
                rovelle_status_name(status)
            );

            return 1;
        }

        printf("%s\n", hash);
        return 0;
    }

    if (argc == 6 &&
        strcmp(argv[1], "blob") == 0 &&
        strcmp(argv[2], "get") == 0) {
        const char *store_path = argv[3];
        const char *hash = argv[4];
        const char *destination_path = argv[5];

        rovelle_status_t status = rovelle_blob_get(
            store_path,
            hash,
            destination_path
        );

        if (status != ROVELLE_OK) {
            fprintf(
                stderr,
                "blob get failed: %s\n",
                rovelle_status_name(status)
            );

            return 1;
        }

        return 0;
    }
    if (argc == 5 &&
        strcmp(argv[1], "manifest") == 0 &&
        strcmp(argv[2], "publish") == 0) {
        char revision[ROVELLE_SHA256_HEX_SIZE];
        const char *store_path = argv[3];
        const char *manifest_path = argv[4];

        rovelle_status_t status = rovelle_manifest_publish(
            store_path,
            manifest_path,
            revision
        );

        if (status != ROVELLE_OK) {
            fprintf(
                stderr,
                "manifest publish failed: %s\n",
                rovelle_status_name(status)
            );

            return 1;
        }

        printf("%s\n", revision);
        return 0;
    }

    fprintf(
        stderr,
        "Unknown command or invalid arguments.\n\n"
    );

    print_help(stderr);
    return 2;
}
