#include <stdio.h>
#include <stdlib.h>

#include <rovelle/hash.h>

static int check_case(
    const char *name,
    const char *hash,
    int expected
)
{
    int actual = rovelle_hash_is_valid(hash);

    if (actual != expected) {
        fprintf(
            stderr,
            "FAIL: %s: expected %d, got %d\n",
            name,
            expected,
            actual
        );

        return 1;
    }

    return 0;
}

int main(void) {
    const char *hash =
            "494f4abfd7ce18ad9e8dd56cf78eb438"
            "cb5806119c8d52e2da7bc00d8b456562";


    const char *hash_63_characters =
            "494f4abfd7ce18ad9e8dd56cf78eb438"
            "cb5806119c8d52e2da7bc00d8b45656";
    const char *hash_65_characters =
            "494f4abfd7ce18ad9e8dd56cf78eb438"
            "cb5806119c8d52e2da7bc00d8b456562a";
    const char *hash_with_g =
            "494f4abfd7ce18ad9e8dd56cf78eb438"
            "cb5806119c8d52e2dg7bc00d8b456562";
    const char *hash_with_uppercase_a =
            "494f4Abfd7ce18ad9e8dd56cf78eb438"
            "cb5806119c8d52e2da7bc00d8b456562";
    const char *hash_with_space =
            "494f4abfd7c 18ad9e8dd56cf78eb438"
            "cb5806119c8d52e2da7bc00d8b456562";

    int failures = 0;

    failures += check_case("valid lowercase hash", hash, 1);
    failures += check_case("null hash", NULL, 0);
    failures += check_case("63-character hash", hash_63_characters, 0);
    failures += check_case("65-character hash", hash_65_characters, 0);
    failures += check_case("hash containing g", hash_with_g, 0);
    failures += check_case(
        "hash containing uppercase A",
        hash_with_uppercase_a,
        0
    );
    failures += check_case(
        "hash containing space",
        hash_with_space,
        0
    );

    if (failures != 0) {
        fprintf(stderr, "%d checks failed\n", failures);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
