// TRN v1 - Transition actor data extracted from OoT scene headers.
// Each entry connects two rooms at a world-space position.
// Used for automatic room loading when the player approaches a doorway.

#pragma once
#include <stdint.h>

namespace TRN {

struct Header {
    uint8_t  magic[4];         // "TRN\x01"
    uint16_t num_transitions;
    uint16_t reserved;
};
static_assert(sizeof(Header) == 8);

struct Entry {
    int8_t   front_room;
    int8_t   back_room;
    int16_t  id;
    int16_t  pos_x, pos_y, pos_z;
    int16_t  rot_y;
    int16_t  params;
    uint16_t reserved;
};
static_assert(sizeof(Entry) == 16);

inline const Header* header(const uint8_t* data) {
    return reinterpret_cast<const Header*>(data);
}

inline const Entry* entries(const uint8_t* data) {
    return reinterpret_cast<const Entry*>(data + sizeof(Header));
}

}  // namespace TRN
