#include "nwos_internal.h"

static void nw_sha256_init(nw_sha256_ctx_t* ctx);
static void nw_sha256_update(nw_sha256_ctx_t* ctx, const uint8_t* data, uint32_t len);
static void nw_sha256_final(nw_sha256_ctx_t* ctx, uint8_t hash[32]);

static void nw_sha256_init(nw_sha256_ctx_t* ctx)
{
    ctx->datalen = 0;
    ctx->bitlen = 0;

    ctx->state[0] = 0x6a09e667;
    ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372;
    ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f;
    ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab;
    ctx->state[7] = 0x5be0cd19;
}

static void nw_sha256_update(nw_sha256_ctx_t* ctx,
                             const uint8_t* data,
                             uint32_t len)
{
    for (uint32_t i = 0; i < len; i++)
    {
        ctx->data[ctx->datalen++] = data[i];

        if (ctx->datalen == 64)
        {
            nw_sha256_transform(ctx, ctx->data);
            ctx->bitlen += 512;
            ctx->datalen = 0;
        }
    }
}

static void nw_sha256_final(nw_sha256_ctx_t* ctx,
                            uint8_t hash[32])
{
    uint32_t i = ctx->datalen;

    ctx->data[i++] = 0x80;

    if (i > 56)
    {
        while (i < 64)
            ctx->data[i++] = 0;

        nw_sha256_transform(ctx, ctx->data);

        for (i = 0; i < 56; i++)
            ctx->data[i] = 0;
    }
    else
    {
        while (i < 56)
            ctx->data[i++] = 0;
    }

    ctx->bitlen += (uint64_t)ctx->datalen * 8;

    for (i = 0; i < 8; i++)
        ctx->data[63 - i] =
            (uint8_t)(ctx->bitlen >> (i * 8));

    nw_sha256_transform(ctx, ctx->data);

    for (i = 0; i < 32; i++)
    {
        hash[i] = (uint8_t)(
            ctx->state[i / 4] >> (24 - (i % 4) * 8)
        );
    }
}

void nw_password_hash(const char* password,
                             const char* salt,
                             char output[65])
{
    static const char hex[] = "0123456789abcdef";
    nw_sha256_ctx_t ctx;
    uint8_t digest[32];
    uint32_t salt_len = 0;
    uint32_t pass_len = 0;

    while (salt[salt_len] != '\0') salt_len++;
    while (password[pass_len] != '\0') pass_len++;

    nw_sha256_init(&ctx);
    nw_sha256_update(&ctx, (const uint8_t*)salt, salt_len);

    const uint8_t separator = ':';
    nw_sha256_update(&ctx, &separator, 1);

    nw_sha256_update(&ctx, (const uint8_t*)password, pass_len);
    nw_sha256_final(&ctx, digest);

    for (int i = 0; i < 32; i++)
    {
        output[i * 2]     = hex[digest[i] >> 4];
        output[i * 2 + 1] = hex[digest[i] & 0x0F];
    }

    output[64] = '\0';
}

void make_root_salt(char output[17])
{
    static const char hex[] = "0123456789abcdef";

    for (int block = 0; block < 2; block++)
    {
        uint32_t lo, hi;

        __asm__ volatile ("rdtsc"
                          : "=a"(lo), "=d"(hi));

        uint32_t value =
            rand_simple() ^ lo ^ (hi * 0x9E3779B9u);

        for (int j = 0; j < 8; j++)
        {
            output[block * 8 + j] =
                hex[(value >> (28 - j * 4)) & 0x0F];
        }
    }

    output[16] = '\0';
}
