#include <zephyr/ztest.h>
#include <protocol.h>
#include <zephyr/logging/log.h>
#include <errno.h>
#include <stdlib.h>
#include <zephyr/kernel.h>

#define SMALL_SLAB_BLOCK_SIZE 64

K_MEM_SLAB_DEFINE(slab_too_small, 1, 1, 4);

LOG_MODULE_REGISTER(protocol_test, LOG_LEVEL_DBG);

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
    struct protocol_data_pkt pkt = {0};

    for (int i = 0; i < PROTOCOL_MAX_PARAMS; ++i)
    {
        pkt.params[i] = test_params[i];
    }

    pkt.command = test_command;
    pkt.num_params = PROTOCOL_MAX_PARAMS;
    pkt.msg_num = (uint16_t)11111;

    size_t data_buf_size = calc_rq_buf_size(&pkt);
    uint8_t buf[data_buf_size];

    pkt.data = buf;
    pkt.data_len = data_buf_size;

    size_t written = 0;

    int rc = serialise_packet(&pkt);

    zassert_equal(SERIALISE_OK, rc);
    zassert_str_equal(expected_serialised_pkt, pkt.data);
    zassert_equal(expected_crc, pkt.crc, "expected %04x, got %04x", expected_crc, pkt.crc);
}

ZTEST(protocol_test, serialise_packet_params_large)
{
    struct protocol_data_pkt pkt = {0};
    uint16_t crc;


    memcpy(pkt.params, test_params_too_large, sizeof(test_params_too_large));
    pkt.command = test_command;
    pkt.num_params = PROTOCOL_MAX_PARAMS;

    size_t data_buf_size = calc_rq_buf_size(&pkt);
    uint8_t buf[data_buf_size];

    pkt.data = buf;
    pkt.data_len = data_buf_size;

    serialise_ret_t rc = serialise_packet(&pkt);

    zassert_equal(SERIALISE_EXCEED_PAIR_LEN, rc);
}

ZTEST(protocol_test, create_packet_normal)
{
    struct protocol_data_pkt *pkt;
    pkt = protocol_packet_create(test_command, test_params, PROTOCOL_MAX_PARAMS, -1);

    zassert_not_null(pkt);
    zassert_str_equal(test_command, pkt->command);

    for (int i = 0; i < PROTOCOL_MAX_PARAMS; ++i)
    {
        zassert_str_equal(test_params[i].key, pkt->params[i].key);
        zassert_str_equal(test_params[i].value, pkt->params[i].value);
    }
    zassert_equal(PROTOCOL_MAX_PARAMS, pkt->num_params);
}

ZTEST(protocol_test, parse_byte_stream)
{
    const uint8_t test_serialised_pkt_bad[] = "!set_rgb,key:value,red:255,green:11,msg:0#cb7a";
    protocol_ctx_obj_t obj;
    protocol_ctx_t ctx;
    struct k_queue q;

    ctx = protocol_init(&obj, test_serialised_pkt_bad, sizeof(test_serialised_pkt_bad), &q);

    LOG_DBG("Initialised");
    parse(ctx);

    if (ctx->latest == NULL)
    {
        LOG_DBG("null");
    }
    LOG_DBG("OK");

    LOG_DBG("pkt exists??? ptr is: %p", ctx->latest);

    zassert_str_equal("set_rgb", ctx->latest->command);
    zassert_str_equal("red", ctx->latest->params[0].key, "ctx->latest->params[0].key = %s", ctx->latest->params[0].key);
    zassert_str_equal("255", ctx->latest->params[0].value);
    zassert_str_equal("green", ctx->latest->params[1].key, "ctx->latest->params[1].key = %s", ctx->latest->params[1].key);
    zassert_str_equal("11", ctx->latest->params[1].value);

    // char* token;
    // k_stack_pop(&token_stack, (stack_data_t *)token, K_NO_WAIT);
    // LOG_INF("token: %s", token);
}


ZTEST_SUITE(protocol_test, NULL, NULL, NULL, NULL, NULL);
