#include "commands.h"
#include <zephyr/sys/util.h>
#include <string.h>
#include <zephyr/kernel.h>

/**
 * Global array of str/enum values for all the accepted commands
 */
static const command_t valid_commands_enum[] = {
    COMMAND_SET_RGB,
    COMMAND_ACK,
    COMMAND_NACK,
};
static const char *valid_commands_str[] = {
    "set_rgb",
    "ack",
    "nack",
};


/**
 * Global arrays of str/enum values for all the accepted keys
 */
static const key_t valid_keys_enum[] = {
    KEY_RED,
    KEY_GREEN,
    KEY_BLUE,
    KEY_MSGNUM,
};
static const char *valid_keys_str[] = {
    "red",
    "green",
    "blue",
};


/**
 * Which specific keys are valid for which command
 */
static const key_t valid_keys_set_rgb[] = {
    SETRGB_RED,
    SETRGB_GREEN,
    SETRGB_BLUE,
};

static const key_t valid_keys_ack[] = {
    KEY_MSGNUM,
};


command_t cmd_to_enum(char *str)
{
    command_t command = COMMAND_INVALID;
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

key_t key_to_enum(char *str)
{
    key_t key = KEY_INVALID;
    for (int i = 0; i < NUM_KEYS; ++i)
    {
        if (strcmp(valid_keys_str[i], str) == 0)
        {
            key = i;
            return key;
        }
    }
    return key;
}

char* cmd_to_string (command_t command)
{
    switch (command)
    {
        case COMMAND_SET_RGB:
            return "set_rgb";
        case COMMAND_ACK:
            return "ack";
        case COMMAND_NACK:
            return "nack";
        default:
            return NULL;
    }
}

char* key_to_string(key_t key)
{
    switch (key)
    {
        case KEY_RED:
            return "red";
        case KEY_GREEN:
            return "green";
        case KEY_BLUE:
            return "blue";
        case KEY_MSGNUM:
            return "msg";
        default:
            __ASSERT(0, "unreachable");
    }
}

value_t str_to_value(char *str)
{
    char *ptr;
    return (value_t) strtol(str, &ptr, 10);
}

static int validate_kv_set_rgb(key_t key, value_t value)
{
    set_rgb_keys_t param = (set_rgb_keys_t) key;

    switch (param)
    {
        case SETRGB_RED:
        case SETRGB_GREEN:
        case SETRGB_BLUE:
            return (value > UINT8_MAX || value < 0);
        default:
            return -1;

    }
}

/**
 * @brief   Given a command, make sure that the params we have
 *          been given make sense.
 *
 * @param   command :   command to evaluate against
 * @param   key     :   key to evaluate
 * @param   value   :   value to evaluate
 *
 * @return  0 if params are valid
 *          -1 if not
 */
int validate_param_for_command(
    command_t command,
    key_t key,
    value_t value)
{
    switch (command)
    {
        case COMMAND_SET_RGB:
            return validate_kv_set_rgb(key, value);
        case COMMAND_ACK:
        case COMMAND_NACK:
            return validate_kv_ack(key, value);
        case NUM_COMMANDS:
        case COMMAND_INVALID:
            return -1;
        default:
            __ASSERT(0, "unreachable");
    }
}