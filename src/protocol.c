#include <errno.h>
#include <zephyr/random/random.h>
#include <string.h>
#include <stdio.h>
#include <zephyr/sys/crc.h>
#include <zephyr/logging/log.h>
#include <stdlib.h>

#include "protocol.h"

#define PKT_SLAB_BLOCK_SIZE         sizeof(struct protocol_pkt)
#define PKT_SLAB_BLOCK_COUNT        12
#define SLAB_ALIGNMENT              4
#define PKT_TIMEOUT_MSEC            100
#define PKT_RINGBUF_LEN             8
#define RING_BUF_ITEM_SIZE          RING_BUF_ITEM_SIZEOF(struct protocol_pkt*)

#define PROTOCOL_MSG_IDENTIFIER     "msg"
#define PROTOCOL_PREAMBLE           "!"
#define PROTOCOL_KEY_VALUE_SEP      ":"
#define PROTOCOL_ITEM_SEP           ","
#define PROTOCOL_CRC                "#"

K_MEM_SLAB_DEFINE(protocol_pkt_slab, PKT_SLAB_BLOCK_SIZE, PKT_SLAB_BLOCK_COUNT, SLAB_ALIGNMENT);

LOG_MODULE_REGISTER(bbbled_protocol, LOG_LEVEL_DBG);

/**
 * @brief Create a new protocol context
 *
 * @param   ctx         :   The empty context object
 * @param   buffer      :   Buffer pointer. Used for storing data received
 *                          over the interface
 * @param   buffer_size :   Size of the buffer
 *
 * @returns An initialised protocol context
 */
protocol_ctx_t protocol_init(
    protocol_ctx_t ctx,
    uint8_t *buffer,
    size_t buffer_size)
{
    protocol_ctx_t this;

    this = ctx;

    this->rx_buf = buffer;
    this->rx_len = buffer_size;
    this->to_send = NULL;
    this->retry_attempts = PROTOCOL_MAX_MSG_RETRIES;

    return this;
}

/**
 * @brief   Construct an ACK for the given message number
 *
 * @param   msg_num :   Message number to ack
 *
 * @return  An ack packet.
 */
static const pkt_t create_ack(const uint16_t msg_num)
{
    return protocol_packet_create(COMMAND_ACK, NULL, 0, msg_num);
}

/**
 * @brief   Construct a NACK. The message number does not matter.
 *
 * @return  A nack packet.
 */
static const pkt_t create_nack()
{
    return protocol_packet_create(COMMAND_NACK, NULL, 0, -1);
}

/**
 * @brief   Create a random 16-bit number
 * @return  A random 16-bit number
 */
static uint16_t create_msg_num(void)
{
    return sys_rand16_get();
}

// /**
//  * @brief   Calculate the required size a buffer should be for serialisation
//  *          of a protocol packet.
//  *
//  * @param   pkt :   The packet being serialised
//  * @returns The size required for a buffer
//  */
// const size_t calc_rq_buf_size(
//     struct protocol_pkt *pkt)
// {
//     size_t command_len = strlen(pkt->command);

//     /*  '!' + "<command>" + ',' */
//     size_t size =
//         sizeof(PROTOCOL_PREAMBLE) +
//         (sizeof(char) * command_len) +
//         sizeof(uint8_t);


//     /*  "<key0>:<value0>,<key1>:<value1>,..." */
//     for (int i = 0; i < pkt->num_params; ++i)
//     {
//         size += (sizeof(char) * strlen(key_to_string(pkt->params[i].key)));
//         size += sizeof(uint8_t); // ':'
//         size += (sizeof(char) * strlen(valuepkt->params[i].value));
//         size += sizeof(uint8_t); // ','
//     }
//     size += (sizeof(char) * strlen(PROTOCOL_MSG_IDENTIFIER));
//     size += sizeof(uint8_t); // ':'
//     size += (sizeof(char) * PROTOCOL_MAX_MSG_NUM_CHARS); // worst case msg number length (16-bit decimal as char)
//     size += (sizeof(char) * 5); // #<CRC:04x>
//     size += sizeof(uint8_t); // '\0';

//     LOG_DBG("calculated size: %ld", size);
//     return size;
// }

/**
 * @brief Convert a packet to a string
 *
 * @param   pkt     :   packet to convert
 *
 * @returns SERIALISE_INVALID_PKT   :   Invalid packet given
 *          SERIALISE_NO_MEM        :   Packet buffer is not big enough
 *          SERIALISE_EXCEED_PAIR_LEN   Exceeded the size of a key:value pair
 *          SERIALISE_OK            :   Successfully serialised the packet
 *
 */
size_t serialise_packet(
    pkt_t pkt,
    uint8_t *dest,
    size_t dest_size)
{
    struct serial_ctx ctx;

    serialise_ctx_init(&ctx, dest, dest_size, NULL);

    struct kv_pair_adapter adapter = {
        .pairs = pkt->params,
        .num_pairs = pkt->num_params,
        .pair_separator = PROTOCOL_KEY_VALUE_SEP,
        .pair_terminator = PROTOCOL_ITEM_SEP
    };

    struct serial_registry reg[] = {
        {.handler = serialise_padding_char,     .user_data = PROTOCOL_PREAMBLE},
        {.handler = serialise_str,              .user_data = cmd_to_string(pkt->command)},
        {.handler = serialise_padding_char,     .user_data = PROTOCOL_ITEM_SEP},
        {.handler = serialise_key_value_pairs,  .user_data = &adapter},
        {.handler = serialise_str,              .user_data = key_to_string(KEY_MSGNUM)},
        {.handler = serialise_padding_char,     .user_data = PROTOCOL_KEY_VALUE_SEP},
        {.handler = serialise_uint16t_dec,      .user_data = &pkt->msg_num},
        {.handler = serialise_padding_char,     .user_data = PROTOCOL_CRC},
    };

    serialise_handler_register(&ctx, reg, ARRAY_SIZE(reg));

    serialise(&ctx);

    /* CRC afterwards */
    crc_t crc = crc16_ccitt(PROTOCOL_CRC_POLY, ctx.buffer, ctx.bytes_written);
    serialise_uint16t_hex(&ctx, &crc);

    LOG_INF("Serialised packet, data=%s", dest);

    return ctx.bytes_written;
}

/**
 * @brief Verify the CRC of a packet
 *
 * @param   bytes   :   bytes to verify
 * @param   len     :   length of the bytes
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

    /* Find where the CRC starts */
    while (*crc_start++ != '#')
    {
        if (index++ == len) break;
    };

    if (index >= len)
    {
        LOG_WRN("could not find a crc");
        return 1;
    }

    /* convert to int */
    crc = (crc_t) strtol(crc_start, NULL, 16);
    data_len = (crc_start - bytes);

    LOG_DBG("got crc %04x, expected %04x", crc, crc16_ccitt(PROTOCOL_CRC_POLY, bytes, data_len));
    if (crc == crc16_ccitt(PROTOCOL_CRC_POLY, bytes, data_len))
    {
        return 0;
    }

    return -1;
}

/**
 * @brief
 */
uint8_t tokeniser(
    char *str,
    size_t len,
    char token_array[][PROTOCOL_MAX_TOKEN_LEN])
{
    uint8_t index;
    char *search_char = PROTOCOL_ITEM_SEP;
    char *start_token;
    char *end_token;

    for (index = 0; index < PROTOCOL_MAX_NUM_TOKENS; ++index)
    {
        if (strstr(str, search_char) == NULL)
        {
            if (strstr(str, PROTOCOL_CRC) == NULL)
            {
                LOG_DBG("reached end of input");
                break;
            }
            else
            {
                search_char = PROTOCOL_CRC;
            }
        }

        start_token = str;
        end_token = strstr((str), search_char);
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
        str = strstr(str, search_char);
        str++;
    }

    return index;
}

int parse(
    char *str,
    size_t len,
    parsed_data_t data,
    uint16_t *msg_num)
{
    command_t command = COMMAND_INVALID;
    char token_array[PROTOCOL_MAX_NUM_TOKENS][PROTOCOL_MAX_TOKEN_LEN] = {0};
    uint8_t pair_index = 0;
    uint8_t num_tokens = 0;
    uint8_t invalid_params = 0;

    if (str == NULL)
    {
        return -1;
    }

    char id = *str;
    if (id != *PROTOCOL_PREAMBLE)
    {
        LOG_WRN("preamble not found");
        return -1;
    }

    if (verify_crc(str, len))
    {
        LOG_WRN("invalid crc");
        return -1;
    }

    /*  Convert the received bytes into string tokens
        Containing command, params and message number.
        Increment the pointer by one beforehand, we dont
        want the '!' included */
    num_tokens = tokeniser((char*) (str + 1), len - 1, token_array);


    command = cmd_to_enum(token_array[0]);

    /*  Check the command is valid */
    if (command == COMMAND_INVALID)
    {
        LOG_ERR("command invalid");
        return -1;
    }

    /*  We have a valid command, now iterate and
        validate the key:value sent with this
        command */
    for (uint8_t index = 1; index < num_tokens; ++index)
    {
        /*  Find the pointer to the separator */
        char *key_end = strstr(token_array[index], PROTOCOL_KEY_VALUE_SEP);

        if (key_end)
        {
            char *val_start;
            char* val_end;
            struct key_val_pair pair;
            size_t key_len = LEN(key_end, token_array[index]);
            char key_str[PROTOCOL_MAX_KEY_LEN];
            char value_str[PROTOCOL_MAX_KEY_LEN];

            /*  Copy and null-terminate the value */
            memcpy(key_str, token_array[index], (key_len));
            key_str[key_len] = '\0';
            pair.key = key_to_enum(key_str);


            /*  Copy the key */
            val_start = key_end + 1;

            /*  assume the end of the token has been null-terminated
                (should be done in the tokeniser)*/
            while (*key_end++ != '\0');
            val_end = key_end;
            memcpy(value_str, val_start, LEN(val_end, val_start));
            pair.value = str_to_value(value_str);

            /*  Validate the params and get the msg number
                if one has been supplied */
            if (strcmp(key_str, PROTOCOL_MSG_IDENTIFIER) == 0)
            {
                *msg_num = pair.value;
            }
            else if (validate_param_for_command(command, pair.key, pair.value) == 0)
            {
                data->params[pair_index] = pair;
                ++pair_index;
            }
            else
            {
                LOG_WRN("invalid param [%s:%s]", pair.key, pair.value);
                invalid_params = 1;
            }
        }
        else
        {
            break;
        }
    }
    data->command = command;
    data->num_params = pair_index;

    if (*msg_num == -1)
    {
        return -1;
    }

    if (invalid_params)
    {
        return -1;
    }

    return PARSER_OK;
}

void remove_packet(protocol_ctx_t ctx, const uint16_t msg_num)
{
    if (ctx->to_send && ctx->to_send->msg_num == msg_num)
    {
        k_mem_slab_free(&protocol_pkt_slab, ctx->to_send);
        ctx->to_send = NULL;
    }
}

void queue_packet(protocol_ctx_t ctx, const pkt_t pkt)
{
    if (ctx->to_send == NULL)
    {
        ctx->to_send = pkt;
    }
}

void mark_packet_for_resend(protocol_ctx_t ctx)
{
    ctx->to_send->resend = true;
}

pkt_t send_pkt(protocol_ctx_t ctx)
{
    return ctx->to_send;
}

void handle_incoming(
    protocol_ctx_t ctx,
    parsed_data_t data)
{
    __ASSERT(ctx, "Invalid ctx ptr");
    __ASSERT(data, "Invalid data ptr");
    int16_t msg_num = -1;

    int ret = parse((char*) ctx->rx_buf, ctx->rx_len, data, &msg_num);
    if (ret)
    {
        LOG_ERR("Parsing failed");
        return;
    }

    switch (data->command)
    {
        case COMMAND_SET_RGB:
            remove_packet(ctx, msg_num);
            queue_packet(ctx, create_ack(msg_num));
            break;
        case COMMAND_ACK:
            remove_packet(ctx, msg_num);
            break;
        case COMMAND_NACK:
            mark_packet_for_resend(ctx);
            break;
        case COMMAND_INVALID:
            queue_packet(ctx, create_nack());
            break;
    }
}

pkt_t protocol_packet_create(
    command_t command,
    struct key_val_pair params[],
    size_t num_params,
    uint16_t msg_num)
{
    pkt_t pkt;

    /*  First validate params */
    for (uint8_t index = 0; index < num_params; ++index)
    {
        if (validate_param_for_command(command, params[index].key, params[index].value)) return NULL;
    }

    /*  Then allocate memory */
    int rc = k_mem_slab_alloc(&protocol_pkt_slab, (void **)&pkt, K_FOREVER);
    if (rc)
    {
        LOG_ERR("slab memory allocation failed");
        return NULL;
    }

    memset(pkt, 0, sizeof(struct protocol_pkt));

    /*  Now we can populate the packet */
    for (uint8_t index = 0; index < num_params; ++index)
    {
        pkt->params[index] = params[index];
    }
    pkt->command = command;
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

    pkt->resend = false;

    return pkt;
}