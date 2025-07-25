#ifndef BBBLED_B64_H
#define BBBLED_B64_H

#include <zephyr/sys/base64.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int b64_encode(uint8_t *dest, const uint8_t* source);

int b64_decode(uint8_t *dest, const uint8_t* source);

#ifdef __cplusplus
}
#endif


#endif // BBBLED_B64_H