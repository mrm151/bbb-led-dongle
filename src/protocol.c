#include "protocol.h"
#include <zephyr/types.h>
#include <errno.h>
#include <zephyr/random/random.h>
#include <string.h>
#include <stdio.h>

K_QUEUE_DEFINE(q_incoming);
K_QUEUE_DEFINE(q_outgoing);


static void protocol_packet_destroy(protocol_data_pkt_t* packet);


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
    size += (sizeof(char) * PROTOCOL_MAX_MSG_IDENTIFIER_LEN); // worst case msg number length (16-bit decimal as char)
    size += sizeof(uint8_t); // ','
    size += sizeof(uint8_t); // '\0';

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

int serialise_packet(uint8_t *buf, size_t buf_size, const protocol_data_pkt_t *pkt, size_t *written)
{
    // null ptr check
    if (buf == NULL || pkt == NULL || written == NULL)
    {
        return -1;
    }

    size_t command_len = strlen(pkt->command);

    if (buf_size < calc_rq_buf_size(command_len, pkt->params, pkt->num_params))
    {
        return ENOMEM;
    }

    if (*written != 0)
    {
        *written = 0;
    }

    // Copy preamble into buf
    *buf = protocol_preamble;
    (*written)++;

    // Copy command "<command>," into buf
    memcpy((buf + *written), pkt->command, command_len);
    *written += (command_len);
    *(buf + *written) = protocol_item_sep;
    (*written)++;

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
            return -1;
        }

        memcpy((buf + *written), pair, total);
        *written += total;
    }

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
        return -1;
    } ;

    memcpy((buf + *written), msg_num_identifier, strlen(msg_num_identifier));
    *written += strlen(msg_num_identifier);

    // null-terminate
    *(buf + *written) = '\0';

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
    return EPERM;
}

/**
 * @brief Verify the data (command and params) of a packet
 *
 * @param   packet  :   packet to verify
 *
 * @return  0 if the data is valid, -1 if it is not
 */
static int verify_data(const protocol_data_pkt_t* packet)
{
    return EPERM;
}

/**
 * @brief Create a new protocol packet
 *
 * @param   command     :   command to send
 * @param   params      :   parameters to send with the command
 * @param   num_params  :   number of parameters to send
 *
 * @return  Success or failure
 */
int protocol_packet_create(
    const char* command,
    protocol_param_t *params,
    size_t num_params,
    protocol_data_pkt_t *dest)
{
    return EPERM;
}