#include "protocol.h"
#include <zephyr/types.h>

static void protocol_packet_destroy(protocol_packet_t* packet);


/**
 * @brief Construct an ACK for the given message number
 *
 * @param   msg_num :   Message number to ack
 *
 * @return  An ack packet.
 */
static const protocol_packet_t* create_ack(const int msg_num);


/**
 * @brief Construct a NACK for the given message number
 *
 * @param   msg_num :   Message number to nack
 *
 * @return  A nack packet.
 */
static const protocol_packet_t* create_nack(const int msg_num);


/**
 * @return  A random 16-bit number
 */
static uint16_t create_msg_num(void);

/**
 * @brief Convert a packet to a string
 *
 * @param   packet  :   packet to convert
 *
 * @return  String representation of the packet
 */

char* packet_to_string(const protocol_packet_t* packet);

/**
 * @brief parse an incoming byte stream
 *
 * @return  A protocol packet if the stream is valid, NULL otherwise
 */
protocol_packet_t* parse(const uint8_t* bytes, size_t len);

/**
 * @brief Verify the CRC of a packet
 *
 * @param   packet  :   packet to verify
 *
 * @return  0 if the CRC is valid, -1 if it is not
 */
static int verify_crc(const uint8_t* bytes, size_t len, uint8_t crc);

/**
 * @brief Verify the data (command and params) of a packet
 *
 * @param   packet  :   packet to verify
 *
 * @return  0 if the data is valid, -1 if it is not
 */
static int verify_data(const protocol_packet_t* packet);

/**
 * @brief Create a new protocol packet
 *
 * @param   command     :   command to send
 * @param   params      :   parameters to send with the command
 * @param   num_params  :   number of parameters to send
 *
 * @return  A pointer to protocol packet
 */
static protocol_packet_t* protocol_packet_create(
    const char* command,
    struct protocol_param* params,
    size_t num_params);