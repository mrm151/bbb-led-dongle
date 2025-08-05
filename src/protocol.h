#ifndef BBBLED_PROTOCOL_H
#define BBBLED_PROTOCOL_H

#include <zephyr/types.h>
#include <zephyr/kernel.h>

#ifdef __cplusplus
extern "C" {
#endif

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
// Maximum number of params + command and msg number
#define PROTOCOL_MAX_NUM_TOKENS (PROTOCOL_MAX_PARAMS + 2)
#define PROTOCOL_VALID_COMMANDS 1
#define STATIC_STR_ARRAY_LEN(array) (sizeof(array) / sizeof(*array))

static const char* protocol_msg_identifier = "msg";

static const uint8_t protocol_preamble = '!';
static const uint8_t protocol_key_value_sep = ':';
static const uint8_t protocol_item_sep = ',';
static const uint8_t protocol_crc = '#';


typedef enum {
    SET_RGB = 0,
    NUM_COMMANDS,
    ACK,
    NACK,
    INVALID,
} command_t;

typedef enum {
    PARSING_OK = 0,
    INVALID_BYTE_STREAM,
    INVALID_PREAMBLE,
    INVALID_CRC,
    INVALID_TOKENS,
} parser_ret_t;

typedef enum {
    SERIALISE_OK = 0,
    SERIALISE_NO_MEM,
    SERIALISE_INVALID_PKT,
    SERIALISE_EXCEED_PAIR_LEN,
    SERIALISE_EXCEED_MSG_LEN,
} serialise_ret_t;


typedef uint16_t crc_t;


static const char* valid_commands_str[] = {
    "set_rgb",
};

static const char* valid_params_set_rgb[] = {
    "red",
    "green",
    "blue"
};

enum pkt_type {
    PKT_TYPE_DATA = 1,
    PKT_TYPE_ACK,
    PKT_TYPE_NACK,
};

struct key_val_pair{
    char key[PROTOCOL_MAX_KEY_LEN];
    char value[PROTOCOL_MAX_VALUE_LEN];
};


/**
 * @brief Packet structure for the protocol. Each packet should include:
 * @param   command     :   the command being sent (e.g brightness/rainbow/sleep)
 * @param   params      :   the kev:value params that correspond to the command
 * @param   num_params  :   how many params we are sending
 * @param   crc         :   the crc for the data
 */
struct protocol_data_pkt {
    char *command;
    struct key_val_pair params[PROTOCOL_MAX_PARAMS];
    size_t num_params;
    uint16_t msg_num;
    uint8_t *data;
    size_t data_len;
    crc_t crc; // CRC checksum for the message
};

typedef struct k_queue* queue_t;

typedef struct {
    uint8_t *rx_buf;
    size_t rx_len;
    queue_t outbox;
    struct protocol_data_pkt *latest;
} protocol_ctx_obj_t;

typedef protocol_ctx_obj_t* protocol_ctx_t;


struct protocol_data_pkt* protocol_packet_create(
    char* command,
    struct key_val_pair *params,
    size_t num_params,
    uint16_t msg_num);

serialise_ret_t serialise_packet(struct protocol_data_pkt *pkt);

int protocol_ctx_init_pkt(protocol_ctx_t ctx, char* command, struct key_val_pair *params, size_t num_params);

protocol_ctx_t protocol_init(
    protocol_ctx_obj_t *ctx,
    uint8_t *buffer,
    size_t buffer_size,
    struct k_queue *queue);

parser_ret_t parse(protocol_ctx_t ctx);

const size_t calc_rq_buf_size(struct protocol_data_pkt *pkt);

#ifdef __cplusplus
}
#endif

#endif /* BBBLED_PROTOCOL_H */