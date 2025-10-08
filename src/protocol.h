#ifndef _BBBLED_PROTOCOL_H
#define _BBBLED_PROTOCOL_H

#include <zephyr/types.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/ring_buffer.h>

#include "commands.h"
#include "serialise.h"
#include "timer.h"

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

/* Helpful macros */

// Find how many items are in an array
#define STATIC_STR_ARRAY_LEN(array) (sizeof(array) / sizeof(*array))

// Find the length between two pointers
#define LEN(end, start) ((end) - (start))

typedef enum {
    PARSER_OK = 0,
    PARSER_INVALID_BYTES,
    PARSER_INVALID_PREAMBLE,
    PARSER_INVALID_CRC,
    PARSER_INVALID_MSG_NUM,
    PARSER_INVALID_CMD,
    PARSER_INVALID_PARAMS,
} parser_ret_t;

typedef enum {
    SERIALISE_OK = 0,
    SERIALISE_NO_MEM,
    SERIALISE_INVALID_PKT,
    SERIALISE_EXCEED_PAIR_LEN,
    SERIALISE_EXCEED_MSG_LEN,
} serialise_ret_t;


typedef uint16_t crc_t;

enum pkt_type {
    PKT_TYPE_DATA = 1,
    PKT_TYPE_ACK,
    PKT_TYPE_NACK,
};

struct parsed_data{
    command_t command;
    struct key_val_pair params[PROTOCOL_MAX_PARAMS];
    size_t num_params;
};

typedef struct parsed_data* parsed_data_t;

/**
 * @brief Packet structure for the protocol. Each packet should include:
 * @param   command     :   the command being sent
 * @param   params      :   the key:value params that correspond to the command
 * @param   num_params  :   how many params we are sending
 * @param   msg_num     :   msg number for the pkt
 * @param   crc         :   the crc for the data
 * @param   resend      :   if the pkt is marked for resend
 */
struct protocol_pkt {
    command_t command;
    struct key_val_pair params[PROTOCOL_MAX_PARAMS];
    size_t num_params;
    uint16_t msg_num;
    crc_t crc; // CRC checksum for the message
    bool resend;
};

typedef struct protocol_pkt* pkt_t;

typedef void (*timer_cb_t)(timer_t*);

struct protocol_ctx {
    uint8_t *rx_buf;
    size_t rx_len;
    struct protocol_pkt *to_send;
    uint8_t retry_attempts;
    timer_t *resend_timer;
};

typedef struct protocol_ctx* protocol_ctx_t;


/**
 * @brief   Creates a new protocol packet and assigns it a
 *          msg number.
 *
 * @param   command        command to send
 * @param   params         parameters to send with the command
 * @param   num_params     number of parameters included
 * @param   msg_num        assign this msg number to the pkt
 * @retval  Ptr to pkt on success
 * @retval  NULL ptr on failure
 */
pkt_t protocol_packet_create(command_t command, struct key_val_pair *params, size_t num_params, uint16_t msg_num);

/**
 * @brief Convert a packet to a string
 *
 * @param   pkt         packet to convert
 * @param   dest        buffer to copy into
 * @param   dest_size   size of buffer
 *
 * @returns Amount of bytes written to the buffer
 *
 */
size_t serialise_packet(struct protocol_pkt *pkt, uint8_t *dest, size_t dest_size);

/**
 * @brief Create a new protocol context
 *
 * @param   ctx         :   The empty context object
 * @param   buffer      :   Buffer pointer. Used for storing data received over the interface
 * @param   buffer_size :   Size of the buffer
 * @param   timer       :   Pointer to an uninitialised timer
 * @returns An initialised protocol context
 */
void protocol_init(
    protocol_ctx_t ctx,
    uint8_t *buffer,
    size_t buffer_size,
    timer_t *timer);

/**
 * @brief   parse a string, returning its command and params if valid
 *
 * @param   str     string to parse
 * @param   len     lenth of the string
 * @param   data    data to populate
 * @param   msg_num msg number for the parsed data
 *
 * @retval  -1 if failure
 * @retval  0 if successful
 */
int parse(
    char *str,
    size_t len,
    parsed_data_t data,
    uint16_t *msg_num);

void handle_incoming(
    protocol_ctx_t ctx,
    parsed_data_t data);




#ifdef __cplusplus
}
#endif

#endif /* _BBBLED_PROTOCOL_H */