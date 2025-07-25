#include <stddef.h>
#include <stdint.h>
#include "b64.h"

LOG_MODULE_REGISTER(BBBLED_B64, LOG_LEVEL_INF);

int b64_encode(uint8_t *dest, const uint8_t* source)
{
    size_t written;
    int ret;

    ret = base64_encode(dest, sizeof(dest), &written, source, sizeof(source));

    if (ret != 0) {
        LOG_ERR("Base64 encoding failed with error %d", ret);
    }

    return ret;
}

int b64_decode(uint8_t *dest, const uint8_t* source)
{
    size_t written;
    int ret;

    ret = base64_decode(dest, sizeof(dest), &written, source, sizeof(source));

    if (ret != 0) {
        LOG_ERR("Base64 decoding failed with error %d", ret);
    }

    return ret;
}