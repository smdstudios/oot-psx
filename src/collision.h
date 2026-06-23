// Runtime collision detection using COL v1 collision data.
// Brute-force point-in-triangle + plane solve, integer arithmetic only.
// Floor, wall, ceiling, and water box detection.

#pragma once
#include "col.h"
#include <climits>

namespace Collision {

struct FloorResult {
    int32_t  floorY;       // Y coordinate of the floor, or INT32_MIN if none
    uint16_t polyIdx;      // index of the floor polygon, or 0xFFFF
    uint16_t surfaceType;  // surface type of the floor polygon
};

// Minimum normal Y for a polygon to be considered a floor.
// 3277 / 32767 ~= 0.1, i.e. surface tilted < ~84deg from horizontal.
constexpr int16_t FLOOR_NY_MIN = 3277;

// Find the highest floor polygon at world position (px, pz) that is
// at or below queryY.
//
// Plane equation: (nx/32767)*x + (ny/32767)*y + (nz/32767)*z + dist = 0
// Solving for y:  y = -(nx*x + nz*z + dist*32767) / ny
inline FloorResult findFloor(const uint8_t* colData,
                              int32_t px, int32_t pz, int32_t queryY) {
    FloorResult result;
    result.floorY = INT32_MIN;
    result.polyIdx = 0xFFFF;
    result.surfaceType = 0;

    if (!colData) return result;

    const auto* hdr   = COL::header(colData);
    const auto* verts = COL::vertices(colData);
    const auto* polys = COL::polys(colData);
    const int numPolys = hdr->num_polys;

    for (int i = 0; i < numPolys; i++) {
        const auto& p = polys[i];

        // Skip non-floor polygons (walls, ceilings)
        if (p.ny < FLOOR_NY_MIN) continue;

        const auto& v0 = verts[p.v0];
        const auto& v1 = verts[p.v1];
        const auto& v2 = verts[p.v2];

        // Point-in-triangle test on XZ plane via edge cross products.
        // Each value is the 2D cross product of an edge with the vector
        // from that edge's start vertex to the query point.
        int32_t cross0 = (int32_t)(v1.z - pz) * (int32_t)(v0.x - v1.x)
                        - (int32_t)(v1.x - px) * (int32_t)(v0.z - v1.z);
        int32_t cross1 = (int32_t)(v2.z - pz) * (int32_t)(v1.x - v2.x)
                        - (int32_t)(v2.x - px) * (int32_t)(v1.z - v2.z);
        int32_t cross2 = (int32_t)(v0.z - pz) * (int32_t)(v2.x - v0.x)
                        - (int32_t)(v0.x - px) * (int32_t)(v2.z - v0.z);

        // Inside if all cross products have the same sign (handles both windings)
        bool allPos = (cross0 >= 0) && (cross1 >= 0) && (cross2 >= 0);
        bool allNeg = (cross0 <= 0) && (cross1 <= 0) && (cross2 <= 0);
        if (!allPos && !allNeg) continue;

        // Solve plane equation for Y at (px, pz)
        int32_t numerator = -((int32_t)p.nx * px
                            + (int32_t)p.nz * pz
                            + (int32_t)p.dist * 32767);
        int32_t floorY = numerator / (int32_t)p.ny;

        // Accept only floors at or below the query point
        if (floorY > queryY) continue;

        // Keep the highest floor
        if (floorY > result.floorY) {
            result.floorY = floorY;
            result.polyIdx = static_cast<uint16_t>(i);
            result.surfaceType = p.type;
        }
    }

    return result;
}

// ── Wall collision ──────────────────────────────────────────────────────

struct WallResult {
    int32_t pushX;      // total X displacement to apply
    int32_t pushZ;      // total Z displacement to apply
    bool    hit;        // true if any wall was contacted
};

// Player collision cylinder constants (OoT Link dimensions).
constexpr int32_t WALL_RADIUS    = 18;
constexpr int32_t PLAYER_HEIGHT  = 60;  // check height for walls (waist)

// Find all wall polygons near (px, py, pz) and compute push displacement.
// Wall polys have |ny| < FLOOR_NY_MIN (near-vertical normals).
// Uses running accumulators so each wall push is seen by subsequent checks.
// stepUp: ignore walls whose top is within this distance above player feet
//         (allows walking up stairs without getting blocked by step faces).
inline WallResult findWalls(const uint8_t* colData,
                            int32_t px, int32_t py, int32_t pz,
                            int32_t checkHeight, int32_t radius,
                            int32_t stepUp = 0) {
    WallResult result;
    result.pushX = 0;
    result.pushZ = 0;
    result.hit = false;

    if (!colData) return result;

    const auto* hdr   = COL::header(colData);
    const auto* verts = COL::vertices(colData);
    const auto* polys = COL::polys(colData);
    const int numPolys = hdr->num_polys;

    int32_t checkY  = py + checkHeight;
    int32_t resultX = px;
    int32_t resultZ = pz;

    for (int i = 0; i < numPolys; i++) {
        const auto& p = polys[i];

        // 1. Wall filter: skip floors and ceilings
        int16_t absNy = p.ny < 0 ? static_cast<int16_t>(-p.ny) : p.ny;
        if (absNy >= FLOOR_NY_MIN) continue;

        const auto& v0 = verts[p.v0];
        const auto& v1 = verts[p.v1];
        const auto& v2 = verts[p.v2];

        // 2. Y-extent: player height range must overlap poly's vertex Y range
        int16_t minVY = v0.y, maxVY = v0.y;
        if (v1.y < minVY) minVY = v1.y; if (v1.y > maxVY) maxVY = v1.y;
        if (v2.y < minVY) minVY = v2.y; if (v2.y > maxVY) maxVY = v2.y;

        if (py + stepUp > maxVY) continue;  // wall top within step-up range
        if (checkY < minVY) continue;       // player check height below wall bottom

        // 3. Signed plane distance at (resultX, checkY, resultZ)
        int32_t planeDist = ((int32_t)p.nx * resultX
                           + (int32_t)p.ny * checkY
                           + (int32_t)p.nz * resultZ
                           + (int32_t)p.dist * 32767) / 32767;

        // Skip if outside collision radius
        int32_t absDist = planeDist < 0 ? -planeDist : planeDist;
        if (absDist > radius) continue;

        // 4. XZ bounding box check (extended by radius)
        int16_t minX = v0.x, maxX = v0.x;
        if (v1.x < minX) minX = v1.x; if (v1.x > maxX) maxX = v1.x;
        if (v2.x < minX) minX = v2.x; if (v2.x > maxX) maxX = v2.x;

        int16_t minZ = v0.z, maxZ = v0.z;
        if (v1.z < minZ) minZ = v1.z; if (v1.z > maxZ) maxZ = v1.z;
        if (v2.z < minZ) minZ = v2.z; if (v2.z > maxZ) maxZ = v2.z;

        if (resultX < minX - radius || resultX > maxX + radius) continue;
        if (resultZ < minZ - radius || resultZ > maxZ + radius) continue;

        // 5. Push player out of wall along normal
        int32_t displacement = radius - planeDist;
        if (displacement <= 0) continue;

        resultX += ((int32_t)p.nx * displacement) / 32767;
        resultZ += ((int32_t)p.nz * displacement) / 32767;
        result.hit = true;
    }

    result.pushX = resultX - px;
    result.pushZ = resultZ - pz;
    return result;
}

// ── Ceiling collision ───────────────────────────────────────────────────

struct CeilingResult {
    int32_t  ceilingY;     // Y coordinate of the ceiling, or INT32_MAX if none
    uint16_t polyIdx;      // index of the ceiling polygon, or 0xFFFF
};

// Player full height for ceiling search range.
constexpr int32_t CEILING_CHECK = 80;

// Find the lowest ceiling polygon at (px, pz) that is above queryY
// and within checkHeight units above it.
inline CeilingResult findCeiling(const uint8_t* colData,
                                  int32_t px, int32_t pz,
                                  int32_t queryY, int32_t checkHeight) {
    CeilingResult result;
    result.ceilingY = INT32_MAX;
    result.polyIdx = 0xFFFF;

    if (!colData) return result;

    const auto* hdr   = COL::header(colData);
    const auto* verts = COL::vertices(colData);
    const auto* polys = COL::polys(colData);
    const int numPolys = hdr->num_polys;

    for (int i = 0; i < numPolys; i++) {
        const auto& p = polys[i];

        // Ceiling polys have ny <= -FLOOR_NY_MIN (downward-facing normals)
        if (p.ny > -FLOOR_NY_MIN) continue;

        const auto& v0 = verts[p.v0];
        const auto& v1 = verts[p.v1];
        const auto& v2 = verts[p.v2];

        // Point-in-triangle test on XZ plane (same as findFloor)
        int32_t cross0 = (int32_t)(v1.z - pz) * (int32_t)(v0.x - v1.x)
                        - (int32_t)(v1.x - px) * (int32_t)(v0.z - v1.z);
        int32_t cross1 = (int32_t)(v2.z - pz) * (int32_t)(v1.x - v2.x)
                        - (int32_t)(v2.x - px) * (int32_t)(v1.z - v2.z);
        int32_t cross2 = (int32_t)(v0.z - pz) * (int32_t)(v2.x - v0.x)
                        - (int32_t)(v0.x - px) * (int32_t)(v2.z - v0.z);

        bool allPos = (cross0 >= 0) && (cross1 >= 0) && (cross2 >= 0);
        bool allNeg = (cross0 <= 0) && (cross1 <= 0) && (cross2 <= 0);
        if (!allPos && !allNeg) continue;

        // Solve plane equation for Y at (px, pz)
        int32_t numerator = -((int32_t)p.nx * px
                            + (int32_t)p.nz * pz
                            + (int32_t)p.dist * 32767);
        int32_t ceilingY = numerator / (int32_t)p.ny;

        // Accept only ceilings above the query point within check range
        if (ceilingY < queryY) continue;
        if (ceilingY > queryY + checkHeight) continue;

        // Keep the lowest ceiling (closest above player)
        if (ceilingY < result.ceilingY) {
            result.ceilingY = ceilingY;
            result.polyIdx = static_cast<uint16_t>(i);
        }
    }

    return result;
}

// ── Water box detection ─────────────────────────────────────────────────

struct WaterResult {
    int32_t surfaceY;   // Y of water surface, or INT32_MIN if not in any box
    bool    inWater;    // true if player position is at or below surface
};

// Check if (px, py, pz) is inside any water box.
// Water boxes are axis-aligned rectangles with a Y surface height.
inline WaterResult checkWaterBoxes(const uint8_t* colData,
                                    int32_t px, int32_t py, int32_t pz) {
    WaterResult result;
    result.surfaceY = INT32_MIN;
    result.inWater = false;

    if (!colData) return result;

    const auto* hdr = COL::header(colData);
    const auto* wbs = COL::waterBoxes(colData);
    const int numWB = hdr->num_water_boxes;

    for (int i = 0; i < numWB; i++) {
        const auto& wb = wbs[i];
        if (px >= wb.x_min && px <= wb.x_min + wb.x_length &&
            pz >= wb.z_min && pz <= wb.z_min + wb.z_length) {
            result.surfaceY = wb.y_surface;
            result.inWater = (py <= wb.y_surface);
            return result;
        }
    }

    return result;
}

}  // namespace Collision
