#include <zephyr/types.h>
#include <errno.h>
#include <zephyr/random/random.h>
#include <string.h>
#include <stdio.h>
#include <zephyr/sys/crc.h>
#include <zephyr/logging/log.h>
#include <stdlib.h>

#include "protocol.h"

#define PKT_SLAB_BLOCK_SIZE sizeof(struct protocol_data_pkt)
#define PKT_SLAB_BLOCK_COUNT 12
#define SLAB_ALIGNMENT 4
#define PKT_TIMEOUT_MSEC 100
#define PKT_RINGBUF_LEN 8
#define RING_BUF_ITEM_SIZE RING_BUF_ITEM_SIZEOF(struct protocol_data_pkt*)

K_MEM_SLAB_DEFINE(protocol_pkt_slab, PKT_SLAB_BLOCK_SIZE, PKT_SLAB_BLOCK_COUNT, SLAB_ALIGNMENT);


LOG_MODULE_REGISTER(bbbled_protocol, LOG_LEVEL_DBG);

command_t to_enum(char *str)
{
    command_t command = INVALID;
    for (int i = 0; i < NUM_COMMANDS; ++i)
    {
        if (strcmp(valid_commands_str[i], str) == 0)
        {
            command = i;
            return command;
        }
    }
    return command;
}

char* to_string (command_t command)
{
    switch (command)
    {
        case SET_RGB:
            return "set_rgb";
        case ACK:
            return "ack";
        case NACK:
            return "nack";
        default:
            return NULL;
    }
}

protocol_ctx_t protocol_init(
    protocol_ctx_obj_t *ctx,
    uint8_t *buffer,
    size_t buffer_size,
    struct ring_buf *ring,
    void *ring_data,
    uint32_t data_size)
{
    protocol_ctx_t this;

    this = ctx;

    ring_buf_item_init(ring, data_size, (uint32_t*) ring_data);
    LOG_DBG("initialised ring buf - how much space?? %d", ring_buf_item_space_get(ring));

    this->outbox = ring;
    this->rx_buf = buffer;
    this->rx_len = buffer_size;
    this->latest = NULL;
    this->retry_attempts = PROTOCOL_MAX_MSG_RETRIES;

    return this;
}

static void protocol_pkt_dealloc(protocol_ctx_t ctx)
{
    // if (ctx->latest)
    // {
    //     k_mem_slab_free(&protocol_pkt_slab, ctx->latest);
    //     ctx->latest = NULL;
    // }
}

/**
 * @brief Construct an ACK for the given message number
 *
 * @param   msg_num :   Message number to ack
 *
 * @return  An ack packet.
 */
static const struct protocol_data_pkt* create_ack(const uint16_t msg_num)
{
    return protocol_packet_create(ACK, NULL, 0, msg_num);
}

/**
 * @brief Construct a NACK for the given message number
 *
 * @param   msg_num :   Message number to nack
 *
 * @return  A nack packet.
 */
static const struct protocol_data_pkt* create_nack(const uint16_t msg_num)
{
    return protocol_packet_create(NACK, NULL, 0, msg_num);
}

// static void queue_ack(protocol_ctx_t ctx, const uint16_t msg_num)
// {
//     k_queue_get
// }

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
const size_t calc_rq_buf_size(
    struct protocol_data_pkt *pkt)
{
    size_t command_len = strlen(pkt->command);

    /*  '!' + "<command>" + ',' */
    size_t size =
        sizeof(protocol_preamble) +
        (sizeof(char) * command_len) +
        sizeof(uint8_t);


    /*  "<key0>:<value0>,<key1>:<value1>,..." */
    for (int i = 0; i < pkt->num_params; ++i)
    {
        size += (sizeof(char) * strlen(pkt->params[i].key));
        size += sizeof(uint8_t); // ':'
        size += (sizeof(char) * strlen(pkt->params[i].value));
        size += sizeof(uint8_t); // ','
    }
    size += (sizeof(char) * strlen(protocol_msg_identifier));
    size += sizeof(uint8_t); // ':'
    size += (sizeof(char) * PROTOCOL_MAX_MSG_NUM_CHARS); // worst case msg number length (16-bit decimal as char)
    size += (sizeof(char) * 5); // #<CRC:04x>
    size += sizeof(uint8_t); // '\0';

    LOG_DBG("calculated size: %ld", size);
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
serialise_ret_t serialise_packet(
    struct protocol_data_pkt *pkt)
{
    uint16_t offset = 0;
    /*  4 characters for CRC plus 1 for null termination */
    char char_crc[5];
    uint16_t crc;
    size_t key_len;
    size_t value_len;
    size_t total;

    // null ptr check
    if (pkt == NULL)
    {
        return SERIALISE_INVALID_PKT;
    }

    // Determine if buffer supplied is large enough
    size_t command_len = strlen(pkt->command);

    if (pkt->data_len < calc_rq_buf_size(pkt))
    {
        LOG_ERR("supplied buffer not large enough: got %ld expected %ld", pkt->data_len, calc_rq_buf_size(pkt));
        return SERIALISE_NO_MEM;
    }

    LOG_DBG("pkt buf size %ld", sizeof(pkt->data));

    // Copy preamble into buf
    *pkt->data = protocol_preamble;
    offset++;
    LOG_DBG("copied preamble");

    // Copy command "<command>," into buf
    memcpy((pkt->data + offset), pkt->command, command_len);
    offset += (command_len);
    *(pkt->data + offset) = protocol_item_sep;
    offset++;
    LOG_DBG("copied command");

    // Copy params e.g "<key>:<value>," into buf
    char pair[PROTOCOL_MAX_KEY_LEN + PROTOCOL_MAX_VALUE_LEN + 2] = {0};
    for (uint8_t index = 0; index < pkt->num_params; ++index)
    {
        key_len = strlen(pkt->params[index].key);
        value_len = strlen(pkt->params[index].value);
        total = key_len + value_len + 2; // including ':' and ','

        int written_to_pair = snprintf(pair,
                            sizeof(pair),
                            "%s:%s,",
                            pkt->params[index].key,
                            pkt->params[index].value);

        if (written_to_pair >= sizeof(pair))
        {
            // key or value of param was too large
            LOG_ERR("exceeded bounds of pair - maxlen=%ld, wrote %ld bytes", sizeof(pair), written_to_pair);
            return SERIALISE_EXCEED_PAIR_LEN;
        }

        memcpy((pkt->data + offset), pair, total);
        offset += total;
    }
    LOG_DBG("copied params");

    // Copy msg number "msg:<msg_num>," into buf
    char msg_num_identifier[16];
    int written_to_msg = snprintf(msg_num_identifier,
                        sizeof(msg_num_identifier),
                        "%s:%d",
                        protocol_msg_identifier,
                        pkt->msg_num);

    if(written_to_msg >= sizeof(msg_num_identifier))
    {
        // pkt->msg_num was too large
        LOG_ERR("exceeded bounds of msg_num maxlen=16, wrote %d bytes", written_to_msg);
        return SERIALISE_EXCEED_MSG_LEN;
    } ;

    memcpy((pkt->data + offset), msg_num_identifier, strlen(msg_num_identifier));
    offset += strlen(msg_num_identifier);
    LOG_DBG("copied msg num");

    // CRC identifier
    *(pkt->data + offset) = protocol_crc;
    offset++;
    LOG_DBG("copied crc id");

    // CRC
    crc = crc16_ccitt(PROTOCOL_CRC_POLY, pkt->data, offset);

    snprintf(char_crc, sizeof(char_crc), "%04x", crc);
    memcpy((pkt->data + offset), char_crc, strlen(char_crc));

    pkt->crc = crc;

    offset += strlen(char_crc);
    LOG_DBG("copied crc");

    // null-terminate
    *(pkt->data + offset) = '\0';
    offset++;


    LOG_INF("Serialised packet, data=%s", pkt->data);

    return SERIALISE_OK;
}

/**
 * @brief Verify the CRC of a packet
 *
 * @param   packet  :   packet to verify
 *
 * @return  0 if the CRC is valid, -1 if it is not
 */
static int verify_crc(const uint8_t* bytes, size_t len)
{
    const uint8_t *crc_start;
    crc_t crc;
    size_t data_len;
    uint8_t index = 0;

    crc_start = bytes;
    while (*crc_start++ != '#')
    {
        if (index++ == len) break;
    };

    if (index >= len)
    {
        LOG_WRN("could not find a crc");
        return 1;
    }

    crc = (crc_t) strtol(crc_start, NULL, 16);
    data_len = (crc_start - bytes);

    LOG_DBG("got crc %04x, expected %04x", crc, crc16_ccitt(PROTOCOL_CRC_POLY, bytes, data_len));
    if (crc == crc16_ccitt(PROTOCOL_CRC_POLY, bytes, data_len))
    {
        return 0;
    }

    return -1;
}

int validate_params_for_command(
    command_t command,
    struct key_val_pair *pair)
{
    char *end;

    LOG_DBG("Validating params [%s: %s] for command '%d'", pair->key, pair->value, command);
    switch (command)
    {
        case SET_RGB:
            for (uint8_t index = 0; index < STATIC_STR_ARRAY_LEN(valid_params_set_rgb); ++index)
            {
                if (strcmp(valid_params_set_rgb[index], pair->key) == 0)
                {

                    if (strtol(pair->value, &end, 10) < 256)
                    {
                        LOG_DBG("pair->value good: %s", pair->value);
                        return 0;
                    }
                }
            }
            break;
        case ACK:
            return 0;
        default:
            LOG_WRN("unrecognised command : %d", command);
            break;
    }

    return -1;
}

parser_ret_t parse_tokens(
    char token_array[][PROTOCOL_MAX_TOKEN_LEN],
    size_t len,
    parsed_data_t *data,
    uint16_t *msg_num)
{
    command_t command = INVALID;
    char key[PROTOCOL_MAX_KEY_LEN];
    char value[PROTOCOL_MAX_VALUE_LEN];
    struct key_val_pair pairs[PROTOCOL_MAX_PARAMS];
    uint8_t pair_index = 0;

    for (uint8_t index = 0; index < PROTOCOL_VALID_COMMANDS; ++index)
    {
        // The command should always be the first member in the array
        if (strcmp(token_array[0], valid_commands_str[index]) == 0)
        {
            command = to_enum(token_array[0]);
        }
    }

    if (command == INVALID)
    {
        LOG_ERR("command invalid");
        return INVALID_CMD;
    }

    // we have a valid command
    // now extract and validate the key:value pairs
    // for this command

    // Start at 1; 0 is already processed
    for (uint8_t index = 1; index < len; ++index)
    {
        char *key_end = strchr(token_array[index], protocol_key_value_sep);

        if (key_end)
        {
            char *val_start;
            char* val_end;
            char *ptr;
            struct key_val_pair pair;
            size_t key_len = LEN(key_end, token_array[index]);

            /*  Copy and null-terminate the value */
            memcpy(pair.key, token_array[index], (key_len));
            pair.key[key_len] = '\0';


            /*  Copy the key */
            val_start = key_end + 1;

            // assume the end of the token has been null-terminated
            while (*key_end++ != '\0');
            val_end = key_end;
            memcpy(pair.value, val_start, LEN(val_end, val_start));

            /*  Validate the params and get the msg number
                if one has been supplied */
            if (strcmp(pair.key, protocol_msg_identifier) == 0)
            {
                *msg_num = (uint16_t) strtol(pair.value, &ptr, 10);
            }
            else if (validate_params_for_command(command, &pair) == 0)
            {
                data->params[pair_index] = pair;
                ++pair_index;
            }
            else
            {
                LOG_WRN("invalid param [%s:%s]", pair.key, pair.value);
            }
        }
    }
    data->command = command;
    data->num_params = pair_index;
    return PARSING_OK;
}

uint8_t tokeniser(
    char *str, size_t len,
    char token_array[][PROTOCOL_MAX_TOKEN_LEN])
{
    uint8_t index;
    char search_char = protocol_item_sep;
    char *start_token;
    char *end_token;

    for (index = 0; index < PROTOCOL_MAX_NUM_TOKENS; ++index)
    {
        if (strchr(str, search_char) == NULL)
        {
            if (strchr(str, protocol_crc) == NULL)
            {
                LOG_DBG("reached end of input");
                break;
            }
            else
            {
                search_char = protocol_crc;
            }
        }

        start_token = str;
        end_token = strchr((str), search_char);
        size_t token_len = LEN(end_token, start_token);

        if (token_len >= PROTOCOL_MAX_TOKEN_LEN)
        {
            LOG_WRN("token too large [%ld bytes] - skipping", (token_len));
        }
        else
        {
            /*  copy the token into the array and null terminate it */
            memcpy(token_array[index], start_token, (token_len));
            token_array[index][token_len] = '\0';
        }

        /* Consume up to (and including) the next search character */
        str = strchr(str, search_char);
        str++;
    }

    return index;
}

/**
 * @brief parse an incoming byte stream
 *
 * @return  A protocol packet if the stream is valid, NULL otherwise
 */
parser_ret_t parse(protocol_ctx_t ctx, parsed_data_t *data)
{
    char id;
    char *csv;
    char token_array[PROTOCOL_MAX_NUM_TOKENS][PROTOCOL_MAX_TOKEN_LEN] = {0};
    uint8_t num_tokens = 0;
    struct protocol_data_pkt *pkt;
    uint16_t msg_num = -1;

    if (ctx->rx_buf == NULL)
    {
        return INVALID_BYTE_STREAM;
    }

    id = *ctx->rx_buf;
    if (id != protocol_preamble)
    {
        LOG_WRN("preamble not found");
        pkt = protocol_packet_create(NACK, NULL, 0, -1);
    }

    if (verify_crc(ctx->rx_buf, ctx->rx_len))
    {
        LOG_WRN("invalid crc");
        return INVALID_CRC;
    }

    /* Convert the received bytes into string tokens
        Containing command, params and message number.
        Increment the pointer by one beforehand, we dont
        want the '!' included */
    csv = (char*) (ctx->rx_buf + 1);
    num_tokens = tokeniser(csv, ctx->rx_len - 1, token_array);

    parser_ret_t ret = parse_tokens(token_array, num_tokens, data, &msg_num);

    /*  if we have a msg number then ack it */
    if (msg_num != -1)
    {
        struct protocol_data_pkt *ack = protocol_packet_create(ACK, NULL, 0, msg_num);
        int ret = ring_buf_item_put(ctx->outbox,
                         0,
                         0,
                         (uint32_t *)&ack,
                         RING_BUF_ITEM_SIZE);
        LOG_DBG("ring_buf_item_put retruend: %d", ret);
    }
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
static int verify(
    const char* command,
    const struct key_val_pair *params)
{
    return EPERM;
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
struct protocol_data_pkt* protocol_packet_create(
    command_t command,
    struct key_val_pair params[],
    size_t num_params,
    uint16_t msg_num)
{
    struct protocol_data_pkt *pkt;
    crc_t crc;
    size_t written = 0;
    struct key_val_pair param;

    k_mem_slab_alloc(&protocol_pkt_slab, (void **)&pkt, K_NO_WAIT);
    memset(pkt, 0, sizeof(struct protocol_data_pkt));

    for (uint8_t index = 0; index < num_params; ++index)
    {
        param = params[index];
        LOG_DBG("param.key = %s", param.key);
        LOG_DBG("param.value = %s", param.value);

        if (strlen(param.key) > PROTOCOL_MAX_KEY_LEN ||
            strlen(param.value) > PROTOCOL_MAX_VALUE_LEN)
        {
            LOG_WRN("omitting '%s:%s' too long", param.key, param.value);
        }
        else
        {
            pkt->params[index] = param;
        }
    }

    memcpy(pkt->command, to_string(command), PROTOCOL_MAX_CMD_LEN);
    LOG_DBG("pkt->command = %s", pkt->command);

    pkt->num_params = num_params;

    /*  If no message number has been given, generate one */
    if (msg_num == -1)
    {
        pkt->msg_num = create_msg_num();
    }
    else
    {
        pkt->msg_num = msg_num;
    }

    LOG_DBG("pkt->msg_num = %d", pkt->msg_num);


    return pkt;
}