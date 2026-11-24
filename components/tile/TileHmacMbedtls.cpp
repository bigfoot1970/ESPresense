#include "TileResolver.h"

#include <string.h>

#include "mbedtls/md.h"

namespace TileResolver {
// privateId = HMAC-SHA256(seed, pad32(uint32_le ctr))[:8]
void HmacMbedtls(const uint8_t seed[16], uint32_t ctr, uint8_t out[8]) {
    uint8_t msg[32] = {0};
    msg[0] = ctr & 0xff;
    msg[1] = (ctr >> 8) & 0xff;
    msg[2] = (ctr >> 16) & 0xff;
    msg[3] = (ctr >> 24) & 0xff;
    uint8_t mac[32];
    mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), seed, 16, msg, sizeof msg, mac);
    memcpy(out, mac, 8);
}
}  // namespace TileResolver
