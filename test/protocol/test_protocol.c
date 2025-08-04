#include <zephyr/ztest.h>
#include <protocol.h>
#include <zephyr/logging/log.h>
#include <errno.h>
#include <stdlib.h>
#include <zephyr/kernel.h>

#define SMALL_SLAB_BLOCK_SIZE 64

K_MEM_SLAB_DEFINE(slab_too_small, 1, 1, 4);

LOG_MODULE_REGISTER(protocol_test);

static const uint8_t expected_serialised_pkt[] =
    "!dummy_command,brightnesssssss:200000000000000,red:122,blue:242,green:1,pulse:2,gradient:10,hello:67,goodbaye:69,msg:11111#1931";

static const size_t expected_serialised_pkt_bytes = 128;

static const uint16_t expected_crc = 0x1931;

static struct key_val_pair test_params[PROTOCOL_MAX_PARAMS] = {
    {.key = "brightnesssssss", .value = "200000000000000"},
    {.key = "red", .value = "122"},
    {.key = "blue", .value = "242"},
    {.key = "green", .value = "1"},
    {.key = "pulse", .value = "2"},
    {.key = "gradient", .value = "10"},
    {.key = "hello", .value = "67"},
    {.key = "goodbaye", .value = "69"}
};

static struct key_val_pair test_params_too_large[PROTOCOL_MAX_PARAMS] = {
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
    struct protocol_buf buf = {
        .buf = {0},
        .len = 0,
    };

    struct protocol_data_pkt pkt = {0};
    uint16_t crc;

    pkt.params = test_params;
    pkt.command = test_command;
    pkt.num_params = PROTOCOL_MAX_PARAMS;
    pkt.msg_num = (uint16_t)11111;
    pkt.data = &buf;

    size_t written = 0;

    int rc = serialise_packet(&pkt, &written, &crc);

    zassert_equal(0, rc);
    zassert_equal(expected_serialised_pkt_bytes, written, "expected:%ld, written:%ld", expected_serialised_pkt_bytes, written);
    zassert_str_equal(expected_serialised_pkt, pkt.data->buf);
    zassert_equal(expected_crc, crc, "expected %04x, got %04x", expected_crc, crc);
}

ZTEST(protocol_test, serialise_packet_params_large)
{
    struct protocol_buf buf = {
        .buf = {0},
        .len = 0
    };

    struct protocol_data_pkt pkt = {0};
    uint16_t crc;

    pkt.params = test_params_too_large;
    pkt.command = test_command;
    pkt.num_params = PROTOCOL_MAX_PARAMS;
    pkt.data = &buf;

    size_t written = 0;

    int rc = serialise_packet(&pkt, &written, &crc);

    zassert_equal(-2, rc);
    // Some bytes have been written
    zassert_true(0 < written);
}

ZTEST(protocol_test, create_packet_normal)
{
    struct protocol_data_pkt *pkt;
    pkt = protocol_packet_create(test_command, test_params, PROTOCOL_MAX_PARAMS);

    zassert_not_null(pkt);
    zassert_str_equal(test_command, pkt->command);
    zassert_equal(test_params, pkt->params);
    zassert_equal(PROTOCOL_MAX_PARAMS, pkt->num_params);
    zassert_equal(expected_serialised_pkt_bytes, pkt->data->len);

    // zassert_equal(1, k_mem_slab_num_used_get(pkt->slab));
    // zassert_equal(1, k_mem_slab_num_used_get(pkt->data->slab));

    // // Manually free memory
    // k_mem_slab_free(pkt->data->slab, pkt->data);
    // zassert_equal(0, k_mem_slab_num_used_get(pkt->data->slab));
    // k_mem_slab_free(pkt->slab, pkt);
    // zassert_equal(0, k_mem_slab_num_used_get(pkt->slab));
}

ZTEST(protocol_test, parse_byte_stream)
{
    const uint8_t test_serialised_pkt_bad[] = "!set_rgb,key:value,aaaa:1111,msg:0#d56a";
    struct protocol_ctx ctx;
    ctx.rx_buf = test_serialised_pkt_bad;
    ctx.rx_len = sizeof(test_serialised_pkt_bad);

    parse(&ctx);

    // char* token;
    // k_stack_pop(&token_stack, (stack_data_t *)token, K_NO_WAIT);
    // LOG_INF("token: %s", token);
}


ZTEST_SUITE(protocol_test, NULL, NULL, NULL, NULL, NULL);
