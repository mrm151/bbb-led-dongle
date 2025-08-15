#include <zephyr/ztest.h>
#include <protocol.h>
#include <zephyr/logging/log.h>
#include <errno.h>
#include <stdlib.h>
#include <zephyr/kernel.h>


LOG_MODULE_REGISTER(protocol_test, LOG_LEVEL_DBG);

static const uint8_t expected_serialised_pkt[] =
    "!set_rgb,brightnesssssss:200000000000000,red:122,blue:242,green:1,pulse:2,gradient:10,hello:67,goodbaye:69,msg:11111#e9e1";

static const size_t expected_serialised_pkt_bytes = 128;

static const uint16_t expected_crc = 0xe9e1;

static command_t test_command = COMMAND_SET_RGB;
static const char test_command_str[] = "set_rgb";

static struct protocol_ctx *initialised_ctx = {0};

const uint8_t byte_stream_valid[] = "!set_rgb,red:255,green:11,msg:10#2d0d";
const uint8_t byte_stream_valid_msg_num = 0;
static const int num_valid_params = 2;
static struct key_val_pair byte_stream_valid_params[2] = {
    {.key = SETRGB_RED, .value = 255},
    {.key = SETRGB_GREEN, .value = 11},
};

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

    char *expected = "!set_rgb,red:255,green:11,msg:15#04b5";

    int written = serialise_packet(&pkt, buf, 256);

    zassert_str_equal(expected, buf);
    zassert_equal(strlen(expected), written);
}

ZTEST_SUITE(protocol_test, NULL, NULL, NULL, NULL, NULL);
