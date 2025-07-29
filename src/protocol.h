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
#define PROTOCOL_CRC_POLY 0x1021 // CCITT poly
#define PROTOCOL_MAX_MSG_IDENTIFIER_LEN 16


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


/**
 * @brief Packet structure for the protocol. Each packet should include:
 * @param   command     :   the command being sent (e.g brightness/rainbow/sleep)
 * @param   params      :   the kev:value params that correspond to the command
 * @param   num_params  :   how many params we are sending
 * @param   crc         :   the crc for the data
 */
typedef struct {
    char* command;
    protocol_param_t *params;
    size_t num_params;
    int msg_num;
    uint16_t crc; // CRC checksum for the message
} protocol_data_pkt_t;


int protocol_packet_create(
    const char* command,
    protocol_param_t *params,
    size_t num_params,
    protocol_data_pkt_t *dest);

int serialise_packet(
    uint8_t *buf,
    size_t buf_size,
    const protocol_data_pkt_t *packet,
    size_t *written);


#ifdef __cplusplus
}
#endif

#endif /* BBBLED_PROTOCOL_H */