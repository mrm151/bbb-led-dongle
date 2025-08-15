#include <zephyr/ztest.h>
#include <protocol.h>
#include <zephyr/logging/log.h>
#include <errno.h>
#include <stdlib.h>
#include <zephyr/kernel.h>


LOG_MODULE_REGISTER(protocol_test, LOG_LEVEL_DBG);

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
