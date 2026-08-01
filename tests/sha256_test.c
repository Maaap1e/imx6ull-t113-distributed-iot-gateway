#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../common/crypto/sha256.h"

static void from_hex(const char *hex, uint8_t output[32])
{
    unsigned int index;
    for (index = 0u; index < 32u; index++) {
        unsigned int value;
        assert(sscanf(hex + index * 2u, "%2x", &value) == 1);
        output[index] = (uint8_t)value;
    }
}

int main(void)
{
    static const char abc_hex[] =
        "ba7816bf8f01cfea414140de5dae2223"
        "b00361a396177a9cb410ff61f20015ad";
    uint8_t expected[32];
    uint8_t actual[32];
    from_hex(abc_hex, expected);
    ota_sha256("abc", 3u, actual);
    assert(memcmp(expected, actual, sizeof(actual)) == 0);
    puts("SHA-256 tests passed");
    return 0;
}
