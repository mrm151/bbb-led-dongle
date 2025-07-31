#ifndef BBBLED_PROTOCOL_H
#define BBBLED_PROTOCOL_H

#include <zephyr/types.h>
#include <zephyr/kernel.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PROTOCOL_VERSION 1
#define PROTOCOL_MAX_PARAMS 8
#define PROTOCOL_MAX_KEY_LEN 16
#define PROTOCOL_MAX_VALUE_LEN 16
#define PROTOCOL_MAX_MSG_RETRIES 5
#define PROTOCOL_CRC_POLY 0x0000 // CCITT poly
// max number of chars in msg:<number> identifier
#define PROTOCOL_MAX_MSG_NUM_CHARS 5
#define PROTOCOL_MAX_CMD_LEN 32
#define PROTOCOL_MAX_DATA_SIZE 305
#define PROTOCOL_RECV_BUF_SIZE 512
#define PROTOCOL_MAX_TOKEN_LEN MAX(PROTOCOL_MAX_CMD_LEN, (PROTOCOL_MAX_KEY_LEN + PROTOCOL_MAX_VALUE_LEN + 1))

static const char* protocol_msg_identifier = "msg";

static const uint8_t protocol_preamble = '!';
static const uint8_t protocol_key_value_sep = ':';
static const uint8_t protocol_item_sep = ',';

static const char* valid_commands[] = {
    "brightness",
    "rainbow",
    "sleep",
    "wake",
    "status"
};

enum pkt_type {
    PKT_TYPE_DATA = 1,
    PKT_TYPE_ACK,
    PKT_TYPE_NACK,
};

typedef struct {
    char key[PROTOCOL_MAX_KEY_LEN];
    char value[PROTOCOL_MAX_VALUE_LEN];
} protocol_param_t;


struct protocol_buf {
    uint8_t buf[PROTOCOL_MAX_DATA_SIZE];
    size_t len;
    struct k_mem_slab *slab;
};

/**
 * @brief Packet structure for the protocol. Each packet should include:
 * @param   command     :   the command being sent (e.g brightness/rainbow/sleep)
 * @param   params      :   the kev:value params that correspond to the command
 * @param   num_params  :   how many params we are sending
 * @param   crc         :   the crc for the data
 */
typedef struct _protocol_data_pkt_t{
    char* command;
    protocol_param_t *params;
    size_t num_params;
    uint16_t msg_num;
    struct protocol_buf *data;
    uint16_t crc; // CRC checksum for the message
    struct k_mem_slab *slab;
} protocol_data_pkt_t;

struct protocol_ctx {
    uint8_t *recv_buf;
    struct k_queue outbox;
    protocol_data_pkt_t *pkt;
};

struct protocol_data {
    char *command;
    protocol_param_t params;
};

protocol_data_pkt_t* protocol_packet_create(
    char* command,
    protocol_param_t *params,
    size_t num_params);

int serialise_packet(
    const protocol_data_pkt_t *pkt,
    size_t *written,
    uint16_t *crc);

int protocol_ctx_init_pkt(struct protocol_ctx *ctx, char* command, protocol_param_t *params, size_t num_params);

int protocol_init_ctx(struct protocol_ctx *ctx);

struct protocol_data parse(uint8_t *bytes, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* BBBLED_PROTOCOL_H */