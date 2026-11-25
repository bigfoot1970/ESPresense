#include "McastCounter.h"

#include <cstdio>
#include <cstring>

#include "esp_netif.h"
#include "esp_netif_net_stack.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "lwip/netif.h"
#include "lwip/pbuf.h"

namespace McastCounter {
namespace {
constexpr int kSlots = 8;
struct Slot {
    uint8_t mac[6];
    uint32_t n;
};

portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
netif_input_fn origInput = nullptr;
struct netif* hooked = nullptr;
uint32_t rx = 0, mcast = 0, bcast = 0, mcBytes = 0;
Slot slots[kSlots] = {};
int64_t windowStartUs = 0;

err_t countingInput(struct pbuf* p, struct netif* inp) {
    if (p && p->len >= 14) {
        const uint8_t* f = static_cast<const uint8_t*>(p->payload);
        const bool group = f[0] & 0x01;
        portENTER_CRITICAL_ISR(&mux);
        rx++;
        if (group) {
            static const uint8_t ff[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
            if (memcmp(f, ff, 6) == 0)
                bcast++;
            else
                mcast++;
            mcBytes += p->tot_len;
            const uint8_t* src = f + 6;
            int freeIdx = -1, minIdx = 0;
            bool found = false;
            for (int i = 0; i < kSlots; i++) {
                if (slots[i].n == 0) {
                    if (freeIdx < 0) freeIdx = i;
                    continue;
                }
                if (memcmp(slots[i].mac, src, 6) == 0) {
                    slots[i].n++;
                    found = true;
                    break;
                }
                if (slots[i].n < slots[minIdx].n) minIdx = i;
            }
            if (!found) {
                // Space-saving style: evict the smallest slot when full.
                int idx = freeIdx >= 0 ? freeIdx : minIdx;
                memcpy(slots[idx].mac, src, 6);
                slots[idx].n = freeIdx >= 0 ? 1 : slots[idx].n + 1;
            }
        }
        portEXIT_CRITICAL_ISR(&mux);
    }
    netif_input_fn next = origInput;
    return next ? next(p, inp) : ERR_IF;
}
}  // namespace

void Ensure() {
    esp_netif_t* sta = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (!sta) return;
    auto* n = static_cast<struct netif*>(esp_netif_get_netif_impl(sta));
    if (!n || !n->input) return;
    if (n == hooked && n->input == countingInput) return;
    if (n->input != countingInput) origInput = n->input;
    hooked = n;
    if (windowStartUs == 0) windowStartUs = esp_timer_get_time();
    n->input = countingInput;
}

Snapshot Take() {
    Snapshot s;
    Slot copy[kSlots];
    int64_t now = esp_timer_get_time();
    portENTER_CRITICAL(&mux);
    s.rx = rx;
    s.mcast = mcast;
    s.bcast = bcast;
    s.mcBytes = mcBytes;
    memcpy(copy, slots, sizeof(slots));
    rx = mcast = bcast = mcBytes = 0;
    memset(slots, 0, sizeof(slots));
    portEXIT_CRITICAL(&mux);
    s.windowMs = windowStartUs ? static_cast<uint32_t>((now - windowStartUs) / 1000) : 0;
    windowStartUs = now;
    int best = -1;
    for (int i = 0; i < kSlots; i++)
        if (copy[i].n && (best < 0 || copy[i].n > copy[best].n)) best = i;
    if (best >= 0) {
        char buf[32];
        const uint8_t* m = copy[best].mac;
        snprintf(buf, sizeof(buf), "%02x:%02x:%02x:%02x:%02x:%02x=%lu", m[0], m[1], m[2], m[3], m[4], m[5],
                 static_cast<unsigned long>(copy[best].n));
        s.top = buf;
    }
    return s;
}
}  // namespace McastCounter
