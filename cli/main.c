#include <stdio.h>
#include <string.h>

#include "rovelle/blob.h"
#include "rovelle/hash.h"
#include "rovelle/manifest.h"
#include "rovelle/status.h"
#include "rovelle/version.h"

typedef enum {
    COMMAND_HELP,
    COMMAND_VERSION,
    COMMAND_HASH,
    COMMAND_BLOB_PUT,
    COMMAND_BLOB_GET,
    COMMAND_MANIFEST_PUBLISH,
    COMMAND_MANIFEST_RESTORE
} command_id_t;

typedef struct {
    command_id_t id;
    int argc;
    const char *name;
    const char *word1;
    const char *word2;
    const char *usage;
    const char *description;
} command_t;

static const command_t commands[] = {
    {COMMAND_HELP, 2, "help", "help", NULL, "help", "Show this help"},
    {COMMAND_VERSION, 2, "version", "version", NULL, "version",
     "Print the project version"},
    {COMMAND_HASH, 3, "hash", "hash", NULL, "hash <file>",
     "Print the SHA-256 of a file"},
    {COMMAND_BLOB_PUT, 5, "blob put", "blob", "put",
     "blob put <store> <file>", "Store a file by its SHA-256"},
    {COMMAND_BLOB_GET, 6, "blob get", "blob", "get",
     "blob get <store> <hash> <destination>", "Restore and verify a blob"},
    {COMMAND_MANIFEST_PUBLISH, 5, "manifest publish", "manifest", "publish",
     "manifest publish <store> <manifest-file>", "Publish manifest"},
    {COMMAND_MANIFEST_RESTORE, 6, "manifest restore", "manifest", "restore",
     "manifest restore <store> <revision> <destination>", "Restore a release by revision"}
};

static const size_t command_count = sizeof(commands) / sizeof(commands[0]);

static const command_t *find_command(int argc, char **argv)
{
    for (size_t index = 0; index < command_count; ++index) {
        const command_t *command = &commands[index];

        if (argc == command->argc &&
            strcmp(argv[1], command->word1) == 0 &&
            (command->word2 == NULL || strcmp(argv[2], command->word2) == 0)) {
            return command;
        }
    }

    return NULL;
}

static void print_help(FILE *stream)
{
    fputs("Usage:\n", stream);

    for (size_t index = 0; index < command_count; ++index) {
        fprintf(stream, "  rovelle %s\n", commands[index].usage);
    }

    fputs("\nCommands:\n", stream);

    for (size_t index = 0; index < command_count; ++index) {
        fprintf(
            stream,
            "  %-41s %s\n",
            commands[index].usage,
            commands[index].description
        );
    }
}

static int is_command_help(int argc, char **argv)
{
    if (strcmp(argv[argc - 1], "--help") != 0) {
        return 0;
    }

    for (size_t index = 0; index < command_count; ++index) {
        const command_t *command = &commands[index];
        int help_argc = command->word2 == NULL ? 3 : 4;

        if (argc == help_argc &&
            strcmp(argv[1], command->word1) == 0 &&
            (command->word2 == NULL || strcmp(argv[2], command->word2) == 0)) {
            return 1;
        }
    }

    return 0;
}

static void print_command_inventory(void)
{
    for (size_t index = 0; index < command_count; ++index) {
        puts(commands[index].name);
    }
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

    if (argc == 2 && strcmp(argv[1], "--commands") == 0) {
        print_command_inventory();
        return 0;
    }

    if (argc == 2 && strcmp(argv[1], "--help") == 0) {
        print_help(stdout);
        return 0;
    }

    if (argc == 2 && strcmp(argv[1], "--version") == 0) {
        printf("rovelle %s\n", rovelle_version());
        return 0;
    }

    const command_t *command = find_command(argc, argv);

    if (command == NULL) {
        fprintf(stderr, "Unknown command or invalid arguments.\n\n");
        print_help(stderr);
        return 2;
    }

    if (command->id == COMMAND_HELP) {
        print_help(stdout);
        return 0;
    }

    if (command->id == COMMAND_VERSION) {
        printf("rovelle %s\n", rovelle_version());
        return 0;
    }

    if (command->id == COMMAND_HASH) {
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

    if (command->id == COMMAND_BLOB_PUT) {
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

    if (command->id == COMMAND_BLOB_GET) {
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
    if (command->id == COMMAND_MANIFEST_PUBLISH) {
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

    if(command->id == COMMAND_MANIFEST_RESTORE) {
        rovelle_status_t status = rovelle_manifest_restore(
                argv[3],
                argv[4],
                argv[5]
            );

        if (status != ROVELLE_OK) {
                fprintf(
                    stderr,
                    "manifest restore failed: %s\n",
                    rovelle_status_name(status)
                );
                return 1;
        }

        return 0;
    }

    return 2;
}
