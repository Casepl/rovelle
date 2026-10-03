#include <stdio.h>
#include <string.h>

#include "rovelle/version.h"

static void print_help(FILE *stream)
{
    fputs("Usage: rovelle <command>\n\n", stream);
    fputs("Commands:\n", stream);
    fputs("  version     Print the project version\n", stream);
    fputs("  help        Show this help\n\n", stream);
    fputs("Options:\n", stream);
    fputs("  --help      Show this help\n", stream);
    fputs("  --version   Print the project version\n", stream);
}

int main(int argc, char **argv)
{
    if (argc == 2 && (strcmp(argv[1], "--help") == 0 ||
                      strcmp(argv[1], "help") == 0)) {
        print_help(stdout);
        return 0;
    }

    if (argc == 2 && (strcmp(argv[1], "version") == 0 ||
                      strcmp(argv[1], "--version") == 0)) {
        printf("rovelle %s\n", rovelle_version());
        return 0;
    }

    if (argc == 1) {
        print_help(stdout);
        return 0;
    }

    fprintf(stderr, "Unknown command or extra arguments.\n\n");
    print_help(stderr);
    return 2;
}
