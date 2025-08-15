#include <zephyr/types.h>
#include "commands.h"

#define SERIALISE_CALLBACKS_MAX UINT8_MAX

/* Serialiser context object definitions */
struct serial_ctx {
    uint8_t *buffer;
    size_t bytes_written;
    size_t buffer_size;
    void *user_data;
    uint8_t max_cb_index;
};

typedef struct serial_ctx* serial_ctx_t;
/* Signature for a serialiser handler */
typedef void (*serialise_handler_t)(serial_ctx_t, void*);

struct serial_registry {
    serialise_handler_t handler;
    void *user_data;
};

/* Adapt the args for serialising kv pairs*/
struct kv_pair_adapter {
    struct key_val_pair *pairs;
    uint8_t num_pairs;
    char *pair_separator;
    char *pair_terminator;
};
typedef struct kv_pair_adapter* kv_pair_adapter_t;

/* public functions */

void serialise_padding_char(serial_ctx_t ctx, void *data);
void serialise_uint16t_dec(serial_ctx_t ctx, void *data);
void serialise_uint16t_hex(serial_ctx_t ctx, void *data);
void serialise_str(serial_ctx_t ctx, void *data);
void serialise_handler_register(serial_ctx_t ctx, struct serial_registry *reg, size_t reg_size);
void serialise_key_value_pairs(serial_ctx_t ctx, void *data);
void serialise(serial_ctx_t ctx);
serial_ctx_t serialise_ctx_init(struct serial_ctx *ctx, uint8_t *buffer, size_t buffer_size, void *user_data);
