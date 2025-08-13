#include <zephyr/ztest.h>
#include <serialise.h>
#include <zephyr/logging/log.h>
#include <errno.h>
#include <stdlib.h>


LOG_MODULE_REGISTER(serialise_test, LOG_LEVEL_DBG);


ZTEST(serialise_test, init)
{
    uint8_t buffer[512];
    struct serial_ctx ctx;

    serialise_ctx_init(&ctx, buffer, 512, NULL);

    zassert_not_null(ctx.buffer);
    zassert_equal(512, ctx.buffer_size);
    zassert_equal(0, ctx.bytes_written);
    zassert_equal(0, ctx.max_cb_index);
}

ZTEST(serialise_test, padding_char)
{
    uint8_t buffer[512] = {0};
    struct serial_ctx ctx;
    char *padding = "!";

    serialise_ctx_init(&ctx, buffer, 512, NULL);

    serialise_padding_char(&ctx, padding);
    zassert_equal('!', ctx.buffer[0]);
    zassert_equal(0, ctx.buffer[1]);
    zassert_equal(1, ctx.bytes_written);
}

ZTEST(serialise_test, string)
{
    uint8_t buffer[512] = {0};
    struct serial_ctx ctx;
    char *str = "msg";

    serialise_ctx_init(&ctx, buffer, 512, NULL);

    serialise_str(&ctx, str);
    zassert_str_equal(str, ctx.buffer);
    zassert_equal(0, ctx.buffer[strlen(str)]);
    zassert_equal(3, ctx.bytes_written);
}

ZTEST(serialise_test, hex_str)
{
    uint8_t buffer[512] = {0};
    struct serial_ctx ctx;
    uint16_t hex = 0x1234;

    serialise_ctx_init(&ctx, buffer, 512, NULL);

    serialise_uint16t_hex(&ctx, &hex);
    zassert_str_equal("1234", ctx.buffer);
    zassert_equal(0, ctx.buffer[5]);
    zassert_equal(4, ctx.bytes_written);
}

ZTEST(serialise_test, dec_str)
{
    uint8_t buffer[512] = {0};
    struct serial_ctx ctx;
    uint16_t dec = 12345;

    serialise_ctx_init(&ctx, buffer, 512, NULL);

    serialise_uint16t_dec(&ctx, &dec);
    zassert_str_equal("12345", ctx.buffer);
    zassert_equal(0, ctx.buffer[6]);
    zassert_equal(5, ctx.bytes_written);
}

ZTEST(serialise_test, multiple)
{
    uint8_t buffer[512] = {0};
    struct serial_ctx ctx;
    char *command = "command";
    char *padding = ",";
    char *msg = "msg";
    char *padding_sep =  ":";
    uint16_t dec = 12345;
    char *padding_crc = "#";
    uint16_t hex = 0x1234;

    char *expected = "command,msg:12345#1234";

    serialise_ctx_init(&ctx, buffer, 512, NULL);

    serialise_str(&ctx, command);
    serialise_padding_char(&ctx, padding);
    serialise_str(&ctx, msg);
    serialise_padding_char(&ctx, padding_sep);
    serialise_uint16t_dec(&ctx, &dec);
    serialise_padding_char(&ctx, padding_crc);
    serialise_uint16t_hex(&ctx, &hex);

    zassert_str_equal(expected, ctx.buffer);
    zassert_equal(strlen(expected), ctx.bytes_written);
    zassert_equal(0, ctx.buffer[strlen(expected)]);
}

ZTEST(serialise_test, kv_pairs)
{
    uint8_t buffer[512] = {0};
    struct serial_ctx ctx;

    struct key_val_pair pairs[3] = {
        {.key = KEY_RED, .value = 255},
        {.key = KEY_GREEN, .value = 234},
        {.key = KEY_BLUE, .value = 123},
    };

    struct kv_pair_adapter adapter = {
        .pairs = pairs,
        .num_pairs = 3,
        .pair_separator = ':',
        .pair_terminator = ',',
    };

    char *expected = "red:255,green:234,blue:123,";

    serialise_ctx_init(&ctx, buffer, 512, NULL);

    serialise_key_value_pairs(&ctx, &adapter);

    zassert_str_equal(expected, ctx.buffer);
    zassert_equal(strlen(expected), ctx.bytes_written);
}

ZTEST(serialise_test, callbacks)
{
    uint8_t buffer[512] = {0};
    struct serial_ctx ctx;

    struct key_val_pair pairs[3] = {
        {.key = KEY_RED, .value = 255},
        {.key = KEY_GREEN, .value = 234},
        {.key = KEY_BLUE, .value = 123},
    };

    struct kv_pair_adapter adapter = {
        .pairs = pairs,
        .num_pairs = 3,
        .pair_separator = ':',
        .pair_terminator = ',',
    };

    uint16_t dec = 12345;
    uint16_t hex = 0x1234;
    struct serial_registry reg[] = {
        {.handler = serialise_padding_char,     .user_data = "!"},
        {.handler = serialise_str,              .user_data = "command"},
        {.handler = serialise_padding_char,     .user_data = ","},
        {.handler = serialise_key_value_pairs,  .user_data = &adapter},
        {.handler = serialise_str,              .user_data = "msg"},
        {.handler = serialise_padding_char,     .user_data = ":"},
        {.handler = serialise_uint16t_dec,      .user_data = &dec},
        {.handler = serialise_padding_char,     .user_data = "#"},
        {.handler = serialise_uint16t_hex,      .user_data = &hex},
    };

    char *expected = "!command,red:255,green:234,blue:123,msg:12345#1234";

    serialise_ctx_init(&ctx, buffer, 512, NULL);

    serialise_handler_register(&ctx, reg, 9);
    serialise(&ctx);

    zassert_str_equal(expected, ctx.buffer);
    zassert_equal(strlen(expected), ctx.bytes_written);
}

ZTEST_SUITE(serialise_test, NULL, NULL, NULL, NULL, NULL);
