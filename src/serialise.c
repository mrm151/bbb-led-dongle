#include "serialise.h"
#include <string.h>
#include <stdio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(serialise, LOG_LEVEL_DBG);

typedef struct {
    serialise_handler_t handler;
    void *user_data;
} serialise_cb_t;

static serialise_cb_t _cb_array[SERIALISE_CALLBACKS_MAX];
static volatile uint8_t cb_index = 0;


void write_to_buffer(uint8_t *dest, uint8_t *src, size_t *written, size_t amount)
{
    memcpy(dest, src, amount);
    *written += amount;
}

void serialise_padding_char(serial_ctx_t ctx, void *data)
{
    char *c = (char*)data;

    write_to_buffer(ctx->buffer + ctx->bytes_written, c, &ctx->bytes_written, 1);
}

void serialise_uint16t_dec(serial_ctx_t ctx, void *data)
{
    uint16_t dec = *(uint16_t*)data;
    uint8_t str_size_16bit_dec = 6;
    char dec_str[str_size_16bit_dec];

    snprintf(dec_str, str_size_16bit_dec, "%d", dec);
    write_to_buffer(ctx->buffer + ctx->bytes_written, dec_str, &ctx->bytes_written, str_size_16bit_dec);
}

void serialise_uint16t_hex(serial_ctx_t ctx, void *data)
{
    uint16_t hex = *(uint16_t*)data;
    uint8_t str_size_16bit_hex = 5;
    char hex_str[str_size_16bit_hex];

    snprintf(hex_str, str_size_16bit_hex, "%04x", hex);
    write_to_buffer(ctx->buffer + ctx->bytes_written, hex_str, &ctx->bytes_written, str_size_16bit_hex);
}

void serialise_str(serial_ctx_t ctx, void *data)
{
    char *str = (char*)data;

    write_to_buffer(ctx->buffer + ctx->bytes_written, str, &ctx->bytes_written, strlen(str));
}

void serialise_handler_register(serial_ctx_t ctx, struct serial_registry *reg, size_t reg_size)
{
    uint8_t max = reg_size + ctx->cb_index;

    for (uint8_t index = ctx->cb_index; index < max; ++index)
    {
        serialise_cb_t *cb = &(_cb_array[index]);
        cb->handler = reg[index].handler;
        cb->user_data = reg[index].user_data;
    }
    cb_index += reg_size;
}

void serialise_key_value_pairs(serial_ctx_t ctx, void *data)
{
    kv_pair_adapter_t adapter = (kv_pair_adapter_t) data;

    for (uint8_t index = 0; index < adapter->num_pairs; ++index)
    {
        serialise_str(ctx, key_to_string(adapter->pairs[index].key));
        serialise_padding_char(ctx, &adapter->pair_separator);
        serialise_uint16t_dec(ctx, &adapter->pairs[index].value);
        serialise_padding_char(ctx, &adapter->pair_terminator);
    }
}

serial_ctx_t serialise_ctx_init(struct serial_ctx *ctx, uint8_t *buffer, size_t buffer_size, void *user_data)
{
    serial_ctx_t this = ctx;

    this->buffer = buffer;
    this->buffer_size = buffer_size;
    this->bytes_written = 0;
    this->user_data = user_data;
    this->cb_index = cb_index;

    return this;
}
