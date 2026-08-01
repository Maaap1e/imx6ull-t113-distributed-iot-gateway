#include "sha256.h"

#include <string.h>

#define ROTR32(value, bits) \
    (((value) >> (bits)) | ((value) << (32u - (bits))))

static const uint32_t k[64] = {
    0x428A2F98u, 0x71374491u, 0xB5C0FBCFu, 0xE9B5DBA5u,
    0x3956C25Bu, 0x59F111F1u, 0x923F82A4u, 0xAB1C5ED5u,
    0xD807AA98u, 0x12835B01u, 0x243185BEu, 0x550C7DC3u,
    0x72BE5D74u, 0x80DEB1FEu, 0x9BDC06A7u, 0xC19BF174u,
    0xE49B69C1u, 0xEFBE4786u, 0x0FC19DC6u, 0x240CA1CCu,
    0x2DE92C6Fu, 0x4A7484AAu, 0x5CB0A9DCu, 0x76F988DAu,
    0x983E5152u, 0xA831C66Du, 0xB00327C8u, 0xBF597FC7u,
    0xC6E00BF3u, 0xD5A79147u, 0x06CA6351u, 0x14292967u,
    0x27B70A85u, 0x2E1B2138u, 0x4D2C6DFCu, 0x53380D13u,
    0x650A7354u, 0x766A0ABBu, 0x81C2C92Eu, 0x92722C85u,
    0xA2BFE8A1u, 0xA81A664Bu, 0xC24B8B70u, 0xC76C51A3u,
    0xD192E819u, 0xD6990624u, 0xF40E3585u, 0x106AA070u,
    0x19A4C116u, 0x1E376C08u, 0x2748774Cu, 0x34B0BCB5u,
    0x391C0CB3u, 0x4ED8AA4Au, 0x5B9CCA4Fu, 0x682E6FF3u,
    0x748F82EEu, 0x78A5636Fu, 0x84C87814u, 0x8CC70208u,
    0x90BEFFFAu, 0xA4506CEBu, 0xBEF9A3F7u, 0xC67178F2u
};

static uint32_t load_be32(const uint8_t *data)
{
    return ((uint32_t)data[0] << 24) |
           ((uint32_t)data[1] << 16) |
           ((uint32_t)data[2] << 8) |
           (uint32_t)data[3];
}

static void store_be32(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)(value >> 24);
    data[1] = (uint8_t)(value >> 16);
    data[2] = (uint8_t)(value >> 8);
    data[3] = (uint8_t)value;
}

static void transform(ota_sha256_ctx_t *ctx, const uint8_t block[64])
{
    uint32_t w[64];
    uint32_t a;
    uint32_t b;
    uint32_t c;
    uint32_t d;
    uint32_t e;
    uint32_t f;
    uint32_t g;
    uint32_t h;
    uint32_t i;

    for (i = 0u; i < 16u; i++) {
        w[i] = load_be32(block + (i * 4u));
    }
    for (i = 16u; i < 64u; i++) {
        uint32_t s0 = ROTR32(w[i - 15u], 7u) ^
                      ROTR32(w[i - 15u], 18u) ^
                      (w[i - 15u] >> 3u);
        uint32_t s1 = ROTR32(w[i - 2u], 17u) ^
                      ROTR32(w[i - 2u], 19u) ^
                      (w[i - 2u] >> 10u);
        w[i] = w[i - 16u] + s0 + w[i - 7u] + s1;
    }

    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];
    e = ctx->state[4];
    f = ctx->state[5];
    g = ctx->state[6];
    h = ctx->state[7];

    for (i = 0u; i < 64u; i++) {
        uint32_t s1 = ROTR32(e, 6u) ^ ROTR32(e, 11u) ^ ROTR32(e, 25u);
        uint32_t choose = (e & f) ^ ((~e) & g);
        uint32_t t1 = h + s1 + choose + k[i] + w[i];
        uint32_t s0 = ROTR32(a, 2u) ^ ROTR32(a, 13u) ^ ROTR32(a, 22u);
        uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
        uint32_t t2 = s0 + majority;

        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }

    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
}

void ota_sha256_init(ota_sha256_ctx_t *ctx)
{
    static const uint32_t initial[8] = {
        0x6A09E667u, 0xBB67AE85u, 0x3C6EF372u, 0xA54FF53Au,
        0x510E527Fu, 0x9B05688Cu, 0x1F83D9ABu, 0x5BE0CD19u
    };

    memcpy(ctx->state, initial, sizeof(initial));
    ctx->bit_count = 0u;
    ctx->block_used = 0u;
}

void ota_sha256_update(ota_sha256_ctx_t *ctx, const void *data, size_t len)
{
    const uint8_t *bytes = (const uint8_t *)data;

    ctx->bit_count += (uint64_t)len * 8u;
    while (len > 0u) {
        size_t available = sizeof(ctx->block) - ctx->block_used;
        size_t take = len < available ? len : available;

        memcpy(ctx->block + ctx->block_used, bytes, take);
        ctx->block_used += take;
        bytes += take;
        len -= take;

        if (ctx->block_used == sizeof(ctx->block)) {
            transform(ctx, ctx->block);
            ctx->block_used = 0u;
        }
    }
}

void ota_sha256_final(ota_sha256_ctx_t *ctx,
                      uint8_t digest[OTA_SHA256_DIGEST_SIZE])
{
    uint64_t bit_count = ctx->bit_count;
    uint32_t i;

    ctx->block[ctx->block_used++] = 0x80u;
    if (ctx->block_used > 56u) {
        memset(ctx->block + ctx->block_used, 0,
               sizeof(ctx->block) - ctx->block_used);
        transform(ctx, ctx->block);
        ctx->block_used = 0u;
    }

    memset(ctx->block + ctx->block_used, 0, 56u - ctx->block_used);
    for (i = 0u; i < 8u; i++) {
        ctx->block[63u - i] = (uint8_t)(bit_count >> (i * 8u));
    }
    transform(ctx, ctx->block);

    for (i = 0u; i < 8u; i++) {
        store_be32(digest + (i * 4u), ctx->state[i]);
    }
    memset(ctx, 0, sizeof(*ctx));
}

void ota_sha256(const void *data, size_t len,
                uint8_t digest[OTA_SHA256_DIGEST_SIZE])
{
    ota_sha256_ctx_t ctx;

    ota_sha256_init(&ctx);
    ota_sha256_update(&ctx, data, len);
    ota_sha256_final(&ctx, digest);
}
