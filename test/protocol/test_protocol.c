#include <zephyr/ztest.h>
#include <protocol.h>
#include <zephyr/logging/log.h>
#include <errno.h>
#include <stdlib.h>

LOG_MODULE_REGISTER(protocol_test);

static const uint8_t expected_serialised_pkt[] =
    "!dummy_command,brightnesssssss:200000000000000,red:122,blue:242,green:1,pulse:2,gradient:10,hello:67,goodbaye:69,msg:11111,6172#";

static const size_t expected_serialised_pkt_bytes = 129;

static const uint16_t expected_crc = 0x6172;

static protocol_param_t test_params[PROTOCOL_MAX_PARAMS] = {
    {.key = "brightnesssssss", .value = "200000000000000"},
    {.key = "red", .value = "122"},
    {.key = "blue", .value = "242"},
    {.key = "green", .value = "1"},
    {.key = "pulse", .value = "2"},
    {.key = "gradient", .value = "10"},
    {.key = "hello", .value = "67"},
    {.key = "goodbaye", .value = "69"}
};

static protocol_param_t test_params_too_large[PROTOCOL_MAX_PARAMS] = {
    {.key = "brightnessssssss", .value = "2000000000000000"},
};

static char* test_command = "dummy_command";

static struct protocol_ctx *initialised_ctx = {0};

// ZTEST(protocol_test, initialise_context)
// {
//     struct protocol_ctx *test_ctx;
//     protocol_ctx_init(test_ctx);

//     zassert_mem_equal(initialised_ctx, test_ctx, "Not equal");
// }


ZTEST(protocol_test, serialise_packet_normal)
{
    uint8_t buf[PROTOCOL_MAX_DATA_SIZE] = {0};
    protocol_data_pkt_t pkt = {0};
    pkt.params = test_params;
    pkt.command = test_command;
    pkt.num_params = PROTOCOL_MAX_PARAMS;
    pkt.msg_num = (uint16_t)11111;
    uint16_t crc;

    size_t written = 0;

    int rc = serialise_packet(buf, sizeof(buf), &pkt, &written, &crc);

    zassert_equal(0, rc);
    zassert_equal(expected_serialised_pkt_bytes, written, "expected:%ld, written:%ld", expected_serialised_pkt_bytes, written);
    zassert_str_equal(expected_serialised_pkt, buf);
    zassert_equal(expected_crc, crc);
}

ZTEST(protocol_test, serialise_packet_too_small)
{
    uint8_t buf[64] = {0};
    protocol_data_pkt_t pkt = {0};
    pkt.params = test_params;
    pkt.command = test_command;
    pkt.num_params = PROTOCOL_MAX_PARAMS;
    uint16_t checksum;

    size_t written = 0;

    int rc = serialise_packet(buf, sizeof(buf), &pkt, &written, &checksum);

    zassert_equal(ENOMEM, rc);
    zassert_equal(0, written);
}

ZTEST(protocol_test, serialise_packet_params_large)
{
    uint8_t buf[256] = {0};
    protocol_data_pkt_t pkt = {0};
    pkt.params = test_params_too_large;
    pkt.command = test_command;
    pkt.num_params = PROTOCOL_MAX_PARAMS;
    uint16_t checksum;

    size_t written = 0;

    int rc = serialise_packet(buf, sizeof(buf), &pkt, &written, &checksum);

    zassert_equal(-1, rc);
    // Some bytes have been written
    zassert_true(0 < written);
}

ZTEST(protocol_test, create_packet_normal)
{
    uint8_t data[PROTOCOL_MAX_DATA_SIZE] = {0};
    protocol_data_pkt_t pkt;

    int rc = protocol_packet_create(test_command, test_params, PROTOCOL_MAX_PARAMS, &pkt, data, sizeof(data));

    zassert_equal(0, rc);
    zassert_str_equal(test_command, pkt.command);
    zassert_equal(test_params, pkt.params);
    zassert_equal(PROTOCOL_MAX_PARAMS, pkt.num_params);
}

ZTEST_SUITE(protocol_test, NULL, NULL, NULL, NULL, NULL);
