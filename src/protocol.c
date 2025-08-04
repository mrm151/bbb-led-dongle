#include "protocol.h"
#include <zephyr/types.h>
#include <errno.h>
#include <zephyr/random/random.h>
#include <string.h>
#include <stdio.h>
#include <zephyr/sys/crc.h>
#include <zephyr/logging/log.h>
#include <stdlib.h>

#define PKT_SLAB_BLOCK_SIZE sizeof(struct protocol_data_pkt)
#define PKT_SLAB_BLOCK_COUNT 12
#define SLAB_ALIGNMENT 4
#define PKT_TIMEOUT_MSEC 100

#define TX_BUF_SLAB_BLOCK_SIZE sizeof(struct protocol_buf)
#define TX_BUF_SLAB_BLOCK_COUNT 12

#define RX_BUF_SLAB_BLOCK_SIZE 360
#define RX_BUF_SLAB_BLOCK_COUNT 12


K_MEM_SLAB_DEFINE(protocol_pkt_slab, PKT_SLAB_BLOCK_SIZE, PKT_SLAB_BLOCK_COUNT, SLAB_ALIGNMENT);
K_MEM_SLAB_DEFINE(protocol_serial_data_slab, TX_BUF_SLAB_BLOCK_SIZE, TX_BUF_SLAB_BLOCK_COUNT, SLAB_ALIGNMENT);
K_MEM_SLAB_DEFINE(rx_buf_slab, RX_BUF_SLAB_BLOCK_SIZE, RX_BUF_SLAB_BLOCK_COUNT, SLAB_ALIGNMENT);



LOG_MODULE_REGISTER(bbbled_protocol, LOG_LEVEL_DBG);

enum valid_command to_enum(char *str)
{
    enum valid_command command = INVALID;
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


char* to_string (enum valid_command command)
{
    switch (command)
    {
        case SET_RGB:
            return "set_rgb";
        default:
            return NULL;
    }
}

static void protocol_pkt_dealloc(struct protocol_ctx *ctx)
{
    if (ctx->pkt)
    {
        k_mem_slab_free(&protocol_serial_data_slab, ctx->pkt->data);
        ctx->pkt->data = NULL;

        k_mem_slab_free(&protocol_pkt_slab, ctx->pkt);
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
static const struct protocol_data_pkt* create_ack(const int msg_num)
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
static const struct protocol_data_pkt* create_nack(const int msg_num)
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
static const size_t calc_rq_buf_size(size_t command_len, struct key_val_pair *params, size_t num_params)
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
    size += (sizeof(char) * 5); // #<CRC:04x>
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
    const struct protocol_data_pkt *pkt,
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
                        "%s:%d",
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

    // CRC identifier
    *(pkt->data->buf + *written) = protocol_crc;
    (*written)++;
    LOG_DBG("copied crc id");

    // CRC
    char char_crc[8];
    uint16_t crc = crc16_ccitt(PROTOCOL_CRC_POLY, pkt->data->buf, *written);

    snprintf(char_crc, sizeof(char_crc), "%04x", crc);
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
 * @brief Verify the CRC of a packet
 *
 * @param   packet  :   packet to verify
 *
 * @return  0 if the CRC is valid, -1 if it is not
 */
static int verify_crc(const uint8_t* bytes, size_t len, uint16_t crc)
{
    LOG_DBG("got crc %04x, expected %04x", crc, crc16_ccitt(PROTOCOL_CRC_POLY, bytes, len));
    if (crc == crc16_ccitt(PROTOCOL_CRC_POLY, bytes, len))
    {
        return 0;
    }

    return -1;
}

int validate_params_for_command(enum valid_command command, char *key, char *value)
{
    char *end;
    LOG_DBG("Validating params [%s: %s] for command '%d'", key, value, command);
    switch (command)
    {
        case SET_RGB:
            for (int index = 0; index < STATIC_STR_ARRAY_LEN(valid_params_set_rgb); ++index)
            {
                if (strcmp(valid_params_set_rgb[index], key) == 0)
                {

                    if (strtol(value, end, 10) < 256)
                    {
                        return 0;
                    }
                }
            }
            break;
        default:
            LOG_WRN("unrecognised command : %d", command);
            break;
    }

    return -1;
}

struct protocol_data_pkt* parse_command_and_params(char token_array[][PROTOCOL_MAX_TOKEN_LEN], size_t len)
{
    if (token_array == NULL || len == 0 || token_array[0] == NULL)
    {
        LOG_WRN("invalid ptr");
        return NULL;
    }

    printf("token_array[0] = %s\n", token_array[0]);
    // printf("token_array[0][0] = %s\n", token_array[0][0]);

    enum valid_command command = INVALID;
    LOG_DBG("");
    for (int index = 0; index < PROTOCOL_VALID_COMMANDS; ++index)
    {
        // The command should always be the first member in the array
        if (strcmp(token_array[0], valid_commands_str[index]) == 0)
        {
            LOG_DBG("hello");
            command = to_enum(token_array[0]);
        }
    }

    LOG_DBG("");
    if (command == INVALID)
    {
        LOG_ERR("command invalid");
        return NULL;
    }

    // we have a valid command
    // now extract and validate the key:value pairs
    // for this command
    char key[PROTOCOL_MAX_KEY_LEN];
    char value[PROTOCOL_MAX_VALUE_LEN];
    struct key_val_pair pairs[PROTOCOL_MAX_PARAMS] = {0};
    int pair_index = 0;
    uint16_t msg_no = 0;

    // Start at 1; 0 is already processed
    for (int index = 1; index < len; ++index)
    {
        char *key_end = strchr(token_array[index], protocol_key_value_sep);
        LOG_DBG("%s", key_end);
        if (key_end)
        {
            LOG_DBG("%ld", key_end - token_array[index]);
            memcpy(key, token_array[index], (key_end - token_array[index]));\
            key[key_end - token_array[index]] = '\0';


            char *val_start = key_end + 1;

            // Assumes that the end of the token is null terminated
            while (*key_end++ != '\0');
            char* val_end = key_end;
            memcpy(value, val_start, (val_end - val_start));

            LOG_DBG("key is %s", key);
            LOG_DBG("value is %s", value);
            strcmp(key, protocol_msg_identifier);
            LOG_DBG("comparing %s == %s", key, protocol_msg_identifier);

            if (strcmp(key, protocol_msg_identifier) == 0)
            {
                LOG_DBG("msg found");
                char *ptr;
                msg_no = (uint16_t) strtol(value, ptr, 10);
            }
            else if (validate_params_for_command(command, key, value) == 0)
            {
                LOG_DBG("key valid");
                struct key_val_pair pair = {.key = key, .value = value};
                pairs[pair_index] = pair;
                ++pair_index;
            }
            else
            {
                LOG_WRN("invalid param [%s:%s]", key, value);
            }
        }
    }

    return protocol_packet_create(to_string(command), pairs, pair_index);
}

/**
 * @brief parse an incoming byte stream
 *
 * @return  A protocol packet if the stream is valid, NULL otherwise
 */
int parse(struct protocol_ctx *ctx)
{
    if (ctx->rx_buf == NULL)
    {
        return;
    }

    char id = *ctx->rx_buf;
    if (id != protocol_preamble)
    {
        LOG_WRN("preamble not found");
    }

    // find the crc
    uint8_t *crc_start = (uint8_t*) strchr((char*) ctx->rx_buf, protocol_crc);

    if (crc_start == NULL)
    {
        LOG_WRN("could not find a crc");
        return;
    }

    LOG_DBG("crc_start %s", crc_start);
    uint16_t crc = (uint16_t) strtol(++crc_start, NULL, 16);

    size_t data_len = (crc_start - ctx->rx_buf);
    uint8_t bytes_no_crc[data_len];
    LOG_INF("Size to copy: %ld", (data_len));

    // Remove the protocol_crc
    memcpy(bytes_no_crc, ctx->rx_buf, data_len);
    bytes_no_crc[data_len] = '\0';

    if (verify_crc((uint8_t*)bytes_no_crc, data_len, crc))
    {
        LOG_WRN("received invalid crc, must NACK");
        return;
    }

    char* csv = bytes_no_crc + 1;
    char *start_token;
    char *end_token;
    char token_array[PROTOCOL_MAX_NUM_TOKENS][PROTOCOL_MAX_TOKEN_LEN] = {0};

    // split the string into parsable tokens
    char search_char = protocol_item_sep;
    int index;
    for (index = 0; index < PROTOCOL_MAX_NUM_TOKENS; ++index)
    {
        if (strchr(csv, search_char) == NULL)
        {
            if (strchr(csv, protocol_crc) == NULL)
            {
                LOG_DBG("reached end of input");
                break;
            }
            else
            {
                search_char = protocol_crc;
            }
        }

        char token[PROTOCOL_MAX_TOKEN_LEN];

        start_token = csv;
        end_token = strchr((csv + 1), search_char); // 1 before ','

        if ((end_token - start_token) >= PROTOCOL_MAX_TOKEN_LEN)
        {
            LOG_WRN("token too large [%ld bytes] - skipping", (end_token - start_token));
        }
        else
        {
            memcpy(token, start_token, (end_token - start_token));
            token[end_token - start_token] = '\0';

            memcpy(token_array[index], token, (end_token - start_token) + 1);
            LOG_INF("token: %s", token);
        }

        // Consume up to the next search char
        csv = strchr(csv, search_char);
        // And consume it also
        csv++;
    }

    // if (index == PROTOCOL_MAX_NUM_TOKENS)
    // {
    //     LOG_WRN("token limit reached, not reading further");
    //     break;
    // }

    struct protocol_data_pkt *pkt = parse_command_and_params(token_array, index);

    return 0;
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
static int verify(const char* command, const struct key_val_pair *params)
{
    return EPERM;
}

static struct protocol_buf* pkt_alloc_buf(void)
{
    struct protocol_buf *buf;
    k_mem_slab_alloc(&protocol_serial_data_slab, (void**)&buf, K_NO_WAIT);
    memset(buf, 0, sizeof(struct protocol_buf));

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
struct protocol_data_pkt* protocol_packet_create(
    char* command,
    struct key_val_pair *params,
    size_t num_params)
{
    struct protocol_data_pkt *pkt;
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

    struct key_val_pair *param = params;
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
    memset(pkt, 0, sizeof(struct protocol_data_pkt));

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

    return pkt;
}