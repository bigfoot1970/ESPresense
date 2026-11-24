#pragma once
// Resolves Tile trackers that rotate their MAC by the rolling privateId in their 0xFEED
// service data (Tile "version 2" advertising, per jose1711/tile-ble-ha):
//   seed      = HMAC-SHA256(authKey, pad32(tileId || "identity"))[:16]   (computed off-node)
//   privateId = HMAC-SHA256(seed, pad32(uint32_le ctr))[:8],  ctr = 0..8640, steps every 15 min
// Only the 16-byte seed is configured on the node; the authKey (which can ring/control the
// Tile) never leaves the server. Setting format: "name:32hex name2:32hex".
#include <stdint.h>
#include <string>
#include <vector>

namespace TileResolver {

constexpr int COUNTER_MAX = 8641;

// Pluggable so the host tests can run without mbedtls.
using HmacFn = void (*)(const uint8_t seed[16], uint32_t ctr, uint8_t out[8]);
void SetHmac(HmacFn fn);
void HmacMbedtls(const uint8_t seed[16], uint32_t ctr, uint8_t out[8]);  // device implementation

// Parse the setting; returns number of valid keys. Not thread-safe vs Resolve (call at setup).
size_t Configure(const std::string& setting);
size_t KeyCount();

// serviceData: raw 0xFEED service data. nowMs: millis(). Returns the key name or nullptr.
// Never blocks for long except the first sync of a key (one full 8641-step search, rate
// limited to one per 60 s across all keys).
const char* Resolve(const uint8_t* serviceData, size_t len, uint32_t nowMs);

// For tests/telemetry.
int LastCounter(size_t key);
uint32_t FullSearches();

}  // namespace TileResolver
