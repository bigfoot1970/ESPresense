#pragma once
#include <cstdint>
#include <string>

// Counts group-addressed (multicast/broadcast) frames the Wi-Fi STA netif receives,
// by wrapping the lwIP netif input function. Diagnostic only: passive, no transmit.
namespace McastCounter {
struct Snapshot {
    uint32_t windowMs = 0;
    uint32_t rx = 0;       // all frames
    uint32_t mcast = 0;    // group-addressed, not broadcast
    uint32_t bcast = 0;    // ff:ff:ff:ff:ff:ff
    uint32_t mcBytes = 0;  // bytes of mcast+bcast
    std::string top;       // "aa:bb:cc:dd:ee:ff=N" source MAC with most group frames
};

// Installs (or re-installs) the hook on the STA netif. Cheap to call repeatedly.
void Ensure();
// Returns counts since the previous Take() and resets them.
Snapshot Take();
}  // namespace McastCounter
