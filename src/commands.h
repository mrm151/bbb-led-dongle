#ifndef _BBBLED_COMMANDS_H
#define _BBBLED_COMMANDS_H
#include <stdio.h>
#include <zephyr/types.h>

typedef enum {
    COMMAND_SET_RGB = 0,
    COMMAND_ACK,
    COMMAND_NACK,
    NUM_COMMANDS,
    COMMAND_INVALID,
} command_t;

typedef enum {
    SETRGB_RED = 0,
    SETRGB_GREEN,
    SETRGB_BLUE
} set_rgb_keys_t;

typedef enum {
    KEY_RED = 0,
    KEY_GREEN,
    KEY_BLUE,
    KEY_MSGNUM,
    NUM_KEYS,
    KEY_INVALID,
} key_t;

typedef uint16_t value_t;

/* kv pair definition */
struct key_val_pair{
    key_t key;
    value_t value;
};

key_t key_to_enum(char *str);
command_t cmd_to_enum(char *str);
char* key_to_string(key_t key);
char* cmd_to_string(command_t command);
value_t str_to_value(char *str);
int validate_param_for_command(command_t command, key_t key, value_t value);

#endif /* _BBBLED_COMMANDS_H */