#include "protocol.h"
#include <zephyr/types.h>
#include <errno.h>
#include <zephyr/random/random.h>
#include <string.h>
#include <stdio.h>
#include <zephyr/sys/crc.h>
#include <zephyr/logging/log.h>

#define PKT_SLAB_BLOCK_SIZE sizeof(protocol_data_pkt_t)
#define PKT_SLAB_BLOCK_COUNT 12
#define SLAB_ALIGNMENT 4
#define PKT_TIMEOUT_MSEC 100

#define TX_BUF_SLAB_BLOCK_SIZE sizeof(struct protocol_buf)
#define TX_BUF_SLAB_BLOCK_COUNT 12

#define RX_BUF_SLAB_BLOCK_SIZE 360
#define RX_BUF_SLAB_BLOCK_COUNT 12


K_MEM_SLAB_DEFINE(protocol_pkt_slab, PKT_SLAB_BLOCK_SIZE, PKT_SLAB_BLOCK_COUNT, SLAB_ALIGNMENT);
K_MEM_SLAB_DEFINE(pkt_buf_slab, TX_BUF_SLAB_BLOCK_SIZE, TX_BUF_SLAB_BLOCK_COUNT, SLAB_ALIGNMENT);
K_MEM_SLAB_DEFINE(rx_buf_slab, RX_BUF_SLAB_BLOCK_SIZE, RX_BUF_SLAB_BLOCK_COUNT, SLAB_ALIGNMENT);



LOG_MODULE_REGISTER(bbbled_protocol, LOG_LEVEL_DBG);


int protocol_alloc_pkt(struct protocol_ctx *ctx, char* command, protocol_param_t *params, size_t num_params)
{
    protocol_data_pkt_t *pkt;

    pkt = protocol_packet_create(command, params, num_params);

    ctx->pkt = pkt;
}

int protocol_init_ctx(struct protocol_ctx *ctx)
{
    uint8_t *buf;

    k_mem_slab_alloc(&rx_buf_slab, (void**)&buf, K_NO_WAIT);

    memset(buf, 0, RX_BUF_SLAB_BLOCK_SIZE);

    ctx->recv_buf = buf;
}

static uint8_t* rx_buf_alloc()
{
    uint8_t *buf;
    k_mem_slab_alloc(&rx_buf_slab, (void**)&buf, K_NO_WAIT);
    memset(buf, 0, RX_BUF_SLAB_BLOCK_SIZE);
    return buf;
}

static void protocol_pkt_dealloc(struct protocol_ctx *ctx)
{
    if (ctx->pkt)
    {
        k_mem_slab_free(ctx->pkt->data->slab, ctx->pkt->data);
        ctx->pkt->data = NULL;

        k_mem_slab_free(ctx->pkt->slab, ctx->pkt);
        ctx->pkt = NULL;
    }
}


/**
 * @brief Construct an ACK for the given message number
 *
 * @param   msg_num :   Message number to ack
 *
 * @return  An ack packet.
 */
static const protocol_data_pkt_t* create_ack(const int msg_num)
{
    return EPERM;
}


/**
 * @brief Construct a NACK for the given message number
 *
 * @param   msg_num :   Message number to nack
 *
 * @return  A nack packet.
 */
static const protocol_data_pkt_t* create_nack(const int msg_num)
{
    return EPERM;
}


/**
 * @return  A random 16-bit number
 */
static uint16_t create_msg_num(void)
{
    return sys_rand16_get();
}


/**
 * @brief Calculate the required size a buffer should be for serialisation
 * of a protocol packet.
 *
 * @param   command_len :   length of the command for the serialised packet
 * @param   params      :   the key:value params for the packet
 * @param   num_params  :   the number of params that the packet contains
 */
static const size_t calc_rq_buf_size(size_t command_len, protocol_param_t *params, size_t num_params)
{
    size_t size =
        sizeof(protocol_preamble) +
        (sizeof(char) * command_len) +
        sizeof(uint8_t); // '!' + "<command>" + ','


    for (int i = 0; i < num_params; ++i)
    {
        size += (sizeof(char) * strlen(params[i].key));
        size += sizeof(uint8_t); // ':'
        size += (sizeof(char) * strlen(params[i].value));
        size += sizeof(uint8_t); // ','
    }
    size += (sizeof(char) * strlen(protocol_msg_identifier));
    size += sizeof(uint8_t); // ':'
    size += (sizeof(char) * PROTOCOL_MAX_MSG_NUM_CHARS); // worst case msg number length (16-bit decimal as char)
    size += sizeof(uint8_t); // ','
    size += sizeof(uint8_t); // '\0';

    LOG_DBG("required size %ld", size);
    return size;
}

/**
 * @brief Convert a packet to a string
 *
 * @param   dest    :   destination buffer
 * @param   pkt     :   packet to convert
 *
 * @return  String representation of the packet
 */

int serialise_packet(
    const protocol_data_pkt_t *pkt,
    size_t *written,
    uint16_t *dest_crc)
{
    // null ptr check
    if (pkt == NULL)
    {
        return -1;
    }

    // Determine if buffer supplied is large enough
    size_t command_len = strlen(pkt->command);
    if (sizeof(pkt->data->buf) < calc_rq_buf_size(command_len, pkt->params, pkt->num_params))
    {
        LOG_ERR("supplied buffer not large enough");
        return ENOMEM;
    }
    LOG_DBG("pkt buf size %ld", sizeof(pkt->data->buf));

    if (*written != 0)
    {
        *written = 0;
    }

    // Copy preamble into buf
    *pkt->data->buf = protocol_preamble;
    (*written)++;
    LOG_DBG("copied preamble");

    // Copy command "<command>," into buf
    memcpy((pkt->data->buf + *written), pkt->command, command_len);
    *written += (command_len);
    *(pkt->data->buf + *written) = protocol_item_sep;
    (*written)++;
    LOG_DBG("copied command");

    // Copy params e.g "<key>:<value>," into buf
    char pair[PROTOCOL_MAX_KEY_LEN + PROTOCOL_MAX_VALUE_LEN + 2] = {0};
    for (int index = 0; index < pkt->num_params; ++index)
    {
        size_t key_len = strlen(pkt->params[index].key);
        size_t value_len = strlen(pkt->params[index].value);
        size_t total = key_len + value_len + 2; // including ':' and ','

        int written_to_pair = snprintf(pair,
                            sizeof(pair),
                            "%s:%s,",
                            pkt->params[index].key,
                            pkt->params[index].value);

        if (written_to_pair >= sizeof(pair))
        {
            // key or value of param was too large
            LOG_ERR("exceeded bounds of pair - maxlen=%ld, wrote %ld bytes", sizeof(pair), written_to_pair);
            return -2;
        }

        memcpy((pkt->data->buf + *written), pair, total);
        *written += total;
    }
    LOG_DBG("copied params");

    // Copy msg number "msg:<msg_num>," into buf
    char msg_num_identifier[16];
    int written_to_msg = snprintf(msg_num_identifier,
                        sizeof(msg_num_identifier),
                        "%s:%d,",
                        protocol_msg_identifier,
                        pkt->msg_num);

    if(written_to_msg >= sizeof(msg_num_identifier))
    {
        // pkt->msg_num was too large
        LOG_ERR("exceeded bounds of msg_num maxlen=16, wrote %d bytes", written_to_msg);
        return -3;
    } ;

    memcpy((pkt->data->buf + *written), msg_num_identifier, strlen(msg_num_identifier));
    *written += strlen(msg_num_identifier);
    LOG_DBG("copied msg num");

    // CRC
    char char_crc[8];
    uint16_t crc = crc16_ccitt(PROTOCOL_CRC_POLY, pkt->data->buf, *written);

    snprintf(char_crc, sizeof(char_crc), "%04x#", crc);
    memcpy((pkt->data->buf + *written), char_crc, strlen(char_crc));

    *dest_crc = crc;

    *written += strlen(char_crc);
    LOG_DBG("copied crc");

    // null-terminate
    *(pkt->data->buf + *written) = '\0';
    (*written)++;


    LOG_INF("Serialised packet, data=%s", pkt->data->buf);

    return 0;
}

/**
 * @brief parse an incoming byte stream
 *
 * @return  A protocol packet if the stream is valid, NULL otherwise
 */
protocol_data_pkt_t* parse(const uint8_t* bytes, size_t len)
{
    return EPERM;
}

/**
 * @brief Verify the CRC of a packet
 *
 * @param   packet  :   packet to verify
 *
 * @return  0 if the CRC is valid, -1 if it is not
 */
static int verify_crc(const uint8_t* bytes, size_t len, uint8_t crc)
{
    return crc == crc16_ccitt(PROTOCOL_CRC_POLY, bytes, len);
}

/**
 * @brief Verify the command and params of a packet.
 * This includes checking that the lengths of theses
 * properties are correst and that the command and parameters
 * are valid.
 *
 * @param   command :   the command for verification
 * @param   params  :   an array of key, value params
 *
 * @return  0 if the data is valid, -1 if it is not
 */
static int verify(const char* command, const protocol_param_t *params)
{
    return EPERM;
}

static struct protocol_buf* pkt_alloc_buf(void)
{
    struct protocol_buf *buf;
    k_mem_slab_alloc(&pkt_buf_slab, (void**)&buf, K_NO_WAIT);
    memset(buf, 0, sizeof(struct protocol_buf));

    buf->slab = &pkt_buf_slab;

    return buf;
}

/**
 * @brief Creates a new protocol packet. Assigns the packet a
 * msg number, serialises its data and sets its CRC.
 *
 * @param   command     :   command to send
 * @param   params      :   parameters to send with the command
 * @param   num_params  :   number of parameters to send
 * @param   dest        :   the packet to populate
 * @param   buf         :   buffer to copy serialised packet into
 * @param   buf_size    :   size of the buffer
 *
 * @return  0 (success) or -1 (fail)
 */
protocol_data_pkt_t* protocol_packet_create(
    char* command,
    protocol_param_t *params,
    size_t num_params)
{
    protocol_data_pkt_t *pkt;
    uint16_t crc = 0;
    size_t written = 0;

    if (command == NULL)
    {
        LOG_WRN("Invalid command");
        return NULL;
    }


    if (strlen(command) > PROTOCOL_MAX_CMD_LEN || num_params > PROTOCOL_MAX_PARAMS)
    {
        size_t len = strlen(command);
        LOG_WRN("Command length=%ld", sizeof(command));
        return NULL;
    }

    protocol_param_t *param = params;
    while (param < params + num_params)
    {
        if (strlen(param->key) > PROTOCOL_MAX_KEY_LEN ||
            strlen(param->value) > PROTOCOL_MAX_VALUE_LEN)
        {
            return NULL;
        }
        param++;
    }

    k_mem_slab_alloc(&protocol_pkt_slab, (void **)&pkt, K_NO_WAIT);
    memset(pkt, 0, sizeof(protocol_data_pkt_t));

    pkt->command = command;
    pkt->params = params;
    pkt->num_params = num_params;
    pkt->msg_num = create_msg_num();

    struct protocol_buf *data;
    data = pkt_alloc_buf();
    pkt->data = data;

    int ret = serialise_packet(pkt, &written, &crc);

    if (ret != 0)
    {
        LOG_ERR("Failed to create packet - buffer size: %ld, bytes written: %ld", PROTOCOL_MAX_DATA_SIZE, written);
        return NULL;
    }

    data->len = written;

    pkt->data = data;
    pkt->crc = crc;
    pkt->slab = &protocol_pkt_slab;

    return pkt;
}