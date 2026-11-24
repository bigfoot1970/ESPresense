#include "TileResolver.h"

#include <string.h>

namespace TileResolver {
namespace {

constexpr int WIN = 3;                       // check predicted ctr -WIN..+WIN
constexpr uint32_t STEP_MS = 15UL * 60000UL;  // counter period
constexpr uint32_t FULL_SEARCH_GAP_MS = 60000UL;
constexpr uint32_t RESYNC_AFTER_MS = 6UL * 3600000UL;  // window missing this long -> allow full search again
constexpr size_t NEG_CACHE = 16;

struct Key {
    std::string name;
    uint8_t seed[16];
    int ctr = -1;           // last matched counter
    uint32_t ctrAtMs = 0;   // when it matched
    int winCenter = -1;     // counter the window below was built around
    uint8_t win[2 * WIN + 1][8];
};

std::vector<Key> keys;
HmacFn hmac = nullptr;
uint8_t neg[NEG_CACHE][8];
size_t negCount = 0, negNext = 0;
uint32_t lastFullMs = 0;
bool fullEver = false;
uint32_t fullSearches = 0;

int hexNib(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

int wrap(int c) {
    c %= COUNTER_MAX;
    return c < 0 ? c + COUNTER_MAX : c;
}

bool isNeg(const uint8_t* pid) {
    for (size_t i = 0; i < negCount; i++)
        if (!memcmp(neg[i], pid, 8)) return true;
    return false;
}

void addNeg(const uint8_t* pid) {
    memcpy(neg[negNext], pid, 8);
    negNext = (negNext + 1) % NEG_CACHE;
    if (negCount < NEG_CACHE) negCount++;
}

void clearNeg() { negCount = negNext = 0; }

int predicted(const Key& k, uint32_t nowMs) {
    return wrap(k.ctr + (int)((nowMs - k.ctrAtMs) / STEP_MS));
}

void matched(Key& k, int ctr, uint32_t nowMs) {
    k.ctr = ctr;
    k.ctrAtMs = nowMs;
}

}  // namespace

void SetHmac(HmacFn fn) { hmac = fn; }

size_t Configure(const std::string& s) {
    keys.clear();
    clearNeg();
    size_t i = 0;
    while (i < s.size()) {
        while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == ',')) i++;
        size_t start = i;
        while (i < s.size() && !(s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == ',')) i++;
        if (i == start) break;
        std::string tok = s.substr(start, i - start);
        auto colon = tok.find(':');
        if (colon == std::string::npos || colon == 0 || tok.size() - colon - 1 != 32) continue;
        Key k;
        k.name = tok.substr(0, colon);
        bool ok = true;
        for (int b = 0; b < 16 && ok; b++) {
            int hi = hexNib(tok[colon + 1 + 2 * b]), lo = hexNib(tok[colon + 2 + 2 * b]);
            if (hi < 0 || lo < 0) ok = false;
            else k.seed[b] = (uint8_t)(hi << 4 | lo);
        }
        if (ok) keys.push_back(k);
    }
    return keys.size();
}

size_t KeyCount() { return keys.size(); }
int LastCounter(size_t key) { return key < keys.size() ? keys[key].ctr : -1; }
uint32_t FullSearches() { return fullSearches; }

const char* Resolve(const uint8_t* sd, size_t len, uint32_t nowMs) {
    if (keys.empty() || !hmac || len < 10 || sd[0] != 2) return nullptr;
    const uint8_t* pid = sd + 2;
    if (isNeg(pid)) return nullptr;

    // 1. Window around each synced key's predicted counter.
    for (auto& k : keys) {
        if (k.ctr < 0) continue;
        int center = predicted(k, nowMs);
        if (center != k.winCenter) {
            for (int d = -WIN; d <= WIN; d++) hmac(k.seed, (uint32_t)wrap(center + d), k.win[d + WIN]);
            k.winCenter = center;
        }
        for (int d = -WIN; d <= WIN; d++)
            if (!memcmp(k.win[d + WIN], pid, 8)) {
                matched(k, wrap(center + d), nowMs);
                return k.name.c_str();
            }
    }

    // 2. Full search for keys never synced (or lost for hours), rate limited.
    bool anyNeed = false;
    for (auto& k : keys)
        if (k.ctr < 0 || nowMs - k.ctrAtMs > RESYNC_AFTER_MS) anyNeed = true;
    if (anyNeed && (!fullEver || nowMs - lastFullMs >= FULL_SEARCH_GAP_MS)) {
        fullEver = true;
        lastFullMs = nowMs;
        fullSearches++;
        uint8_t out[8];
        for (auto& k : keys) {
            if (!(k.ctr < 0 || nowMs - k.ctrAtMs > RESYNC_AFTER_MS)) continue;
            for (int c = 0; c < COUNTER_MAX; c++) {
                hmac(k.seed, (uint32_t)c, out);
                if (!memcmp(out, pid, 8)) {
                    matched(k, c, nowMs);
                    k.winCenter = -1;
                    return k.name.c_str();
                }
            }
        }
        addNeg(pid);  // fully searched, not ours
        return nullptr;
    }
    if (!anyNeed) addNeg(pid);  // every key synced and windows missed: some other Tile
    return nullptr;
}

}  // namespace TileResolver
