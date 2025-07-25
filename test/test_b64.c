#include <gtest/gtest.h>
#include "b64.h"

TEST(B64Test, EncodeDecode) {
    uint8_t msg[] = "Hello, World!";
    uint8_t encoded[256];
    uint8_t decoded[256];

    ASSERT_EQ(b64_encode(encoded, msg), 0);
    ASSERT_EQ(b64_decode(decoded, encoded), 0);
    ASSERT_EQ(decoded, msg);
}
