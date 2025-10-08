#include <zephyr/ztest.h>
#include <protocol.h>
#include <zephyr/logging/log.h>
#include <errno.h>
#include <stdlib.h>
#include <zephyr/kernel.h>


LOG_MODULE_REGISTER(protocol_test, LOG_LEVEL_DBG);


static timer_t test_timer;
static bool timer_expired = false;

static void timer_expiry(timer_t timer)
{
    timer_expired = true;
}


ZTEST(protocol_test, serialise_pkt)
{
    struct protocol_pkt pkt = {
        .params = {
            {.key = SETRGB_RED, .value = 255},
            {.key = SETRGB_GREEN, .value = 11},
        },
        .command = COMMAND_SET_RGB,
        .num_params = 2,
        .msg_num = 15,
    };

    uint8_t buf[256] = {0};

    char *expected = "!set_rgb,red:255,green:11,msg:15#53b5";

    int written = serialise_packet(&pkt, buf, 256);

    zassert_str_equal(expected, buf);
    zassert_equal(strlen(expected), written);
}

ZTEST(protocol_test, parse_pkt)
{
    uint8_t stream[] = "!set_rgb,red:1,green:2,blue:3#463d";
    struct parsed_data parsed = {0};
    uint16_t msg_num = 0;

    parse((char*)stream, ARRAY_SIZE(stream), &parsed, &msg_num);

    zassert_equal(COMMAND_SET_RGB, parsed.command);
    zassert_equal(KEY_RED, parsed.params[0].key);
    zassert_equal(1, parsed.params[0].value);

    zassert_equal(KEY_GREEN, parsed.params[1].key);
    zassert_equal(2, parsed.params[1].value);

    zassert_equal(KEY_BLUE, parsed.params[2].key);
    zassert_equal(3, parsed.params[2].value);
}

ZTEST(protocol_test, parse_ack)
{
    uint8_t stream[] = "!ack,msg:16#0745";
    struct parsed_data parsed = {0};
    uint16_t msg_num = 0;

    parse((char*) stream, ARRAY_SIZE(stream), &parsed, &msg_num);

    zassert_equal(COMMAND_ACK, parsed.command);
    zassert_equal(0, parsed.num_params);
}

ZTEST(protocol_test, parse_nack)
{
    uint8_t stream[] = "!nack,msg:5489#5f6f";
    struct parsed_data parsed = {0};
    uint16_t msg_num = 0;

    parse((char*) stream, ARRAY_SIZE(stream), &parsed, &msg_num);

    zassert_equal(COMMAND_NACK, parsed.command);
    zassert_equal(0, parsed.num_params);
}

ZTEST(protocol_test, handle_incoming_data)
{
    uint8_t buffer[] = "!set_rgb,green:244,red:0,blue:0,msg:48913#a820";
    struct parsed_data parsed = {0};

    struct protocol_ctx ctx;
    timer_t timer;
    protocol_init(&ctx, buffer, ARRAY_SIZE(buffer), &timer);

    handle_incoming(&ctx, &parsed);

    zassert_equal(COMMAND_SET_RGB, parsed.command);
    zassert_equal(3, parsed.num_params);
    zassert_equal(COMMAND_ACK, ctx.to_send->command);
    zassert_equal(48913, ctx.to_send->msg_num);
}


ZTEST(protocol_test, handle_incoming_nack)
{
    uint8_t buffer[] = "!nack,msg:5489#5f6f";
    struct parsed_data parsed = {0};

    struct protocol_ctx ctx;
    timer_t timer;
    protocol_init(&ctx, buffer, ARRAY_SIZE(buffer), &timer);

    struct protocol_pkt pkt = {
        .command = COMMAND_SET_RGB,
    };

    ctx.to_send = &pkt;

    handle_incoming(&ctx, &parsed);

    zassert_true(ctx.to_send->resend);
}

ZTEST(protocol_test, testing)
{
    uint8_t buffer[] = "!et_rgb,green:244,red:0,blue:0,msg:48913#e988";
    struct parsed_data parsed = {0};
    timer_expired = false;

    struct protocol_ctx ctx;
    timer_t timer;
    protocol_init(&ctx, buffer, ARRAY_SIZE(buffer), &timer);

    handle_incoming(&ctx, &parsed);

    zassert_not_null(ctx.to_send);
    zassert_equal(COMMAND_NACK, ctx.to_send->command);
}

ZTEST_SUITE(protocol_test, NULL, NULL, NULL, NULL, NULL);
