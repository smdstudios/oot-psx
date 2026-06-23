// COL v1 - PS1 Collision format
// Binary format for static scene collision, extracted from OoT CollisionHeader.
//
// Layout (all little-endian, 4-byte aligned):
//   Header         16 bytes
//   Vertex[]       num_verts * 6 bytes (padded to 4-byte align)
//   Poly[]         num_polys * 16 bytes
//   WaterBox[]     num_water_boxes * 12 bytes
//
// Normals are s16 where 0x7FFF ≈ 1.0 (same as OoT CollisionPoly).
// Floor polys have ny > 0 (positive Y = up).

#pragma once
#include <stdint.h>

namespace COL {

struct Header {
    uint8_t  magic[4];         // "COL\x01"
    uint16_t num_verts;
    uint16_t num_polys;
    uint16_t num_water_boxes;
    int16_t  min_y;
    int16_t  max_y;
    uint16_t reserved;
};
static_assert(sizeof(Header) == 16);

struct Vertex {
    int16_t x, y, z;
};
static_assert(sizeof(Vertex) == 6);

struct Poly {
    uint16_t v0, v1, v2;
    uint16_t type;         // surface type index
    int16_t  nx, ny, nz;   // normal (0x7FFF ≈ 1.0)
    int16_t  dist;          // plane distance from origin
};
static_assert(sizeof(Poly) == 16);

struct WaterBox {
    int16_t  x_min, y_surface, z_min;
    int16_t  x_length, z_length;
    uint16_t pad;
};
static_assert(sizeof(WaterBox) == 12);

// ── Runtime accessors (zero-copy from binary blob) ──

inline const Header* header(const uint8_t* col) {
    return reinterpret_cast<const Header*>(col);
}

inline const Vertex* vertices(const uint8_t* col) {
    return reinterpret_cast<const Vertex*>(col + sizeof(Header));
}

inline const Poly* polys(const uint8_t* col) {
    const auto* h = header(col);
    uint32_t verts_size = ((h->num_verts * 6u) + 3) & ~3u;
    return reinterpret_cast<const Poly*>(col + sizeof(Header) + verts_size);
}

inline const WaterBox* waterBoxes(const uint8_t* col) {
    const auto* h = header(col);
    return reinterpret_cast<const WaterBox*>(
        reinterpret_cast<const uint8_t*>(polys(col)) + h->num_polys * sizeof(Poly));
}

}  // namespace COL
