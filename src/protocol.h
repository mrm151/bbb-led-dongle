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
#define PROTOCOL_MAX_VALUE_LEN 32
#define PROTOCOL_MAX_MSG_RETRIES 5
#define PROTOCOL_PREAMBLE "!"

K_QUEUE_DEFINE(q_incoming);
K_QUEUE_DEFINE(q_outgoing);

static const char* valid_commands[] = {
    "brightness",
    "rainbow",
    "sleep",
    "wake",
    "status"
};

enum packet_type {
    PACKET_TYPE_DATA = 1,
    PACKET_TYPE_ACK,
    PACKET_TYPE_NACK,
};

struct protocol_param {
    char key[PROTOCOL_MAX_KEY_LEN];
    char value[PROTOCOL_MAX_VALUE_LEN];
};


/**
 * @brief Packet structure for the protocol. Each packet should include:
 * @param   preamble    :   a preamble (currently just one character)
 * @param   command     :   the command being sent (e.g brightness/rainbow/sleep)
 * @param   params      :   the kev:value params that correspond to the command
 * @param   num_params  :   how many params we are sending
 * @param   crc         :   the crc for the data
 * @param   type        :   the packet type. Used internally.
 */
typedef struct {
    char* preamble;
    char* command;
    struct protocol_param params[PROTOCOL_MAX_PARAMS];
    size_t num_params;
    int msg_num;
    uint8_t crc; // CRC checksum for the message
    enum packet_type type;
} protocol_packet_t;


protocol_packet_t* protocol_packet_create(const char* command, struct protocol_param* params, size_t num_params);


#ifdef __cplusplus
}
#endif

#endif /* BBBLED_PROTOCOL_H */