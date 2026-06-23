// OoT PS1 - Scene declarations shared across translation units.

#pragma once

#include "psyqo/application.hh"
#include "psyqo/buffer.hh"
#include "psyqo/cdrom-device.hh"
#include "psyqo/fixed-point.hh"
#include "psyqo/font.hh"
#include "psyqo/fragments.hh"
#include "psyqo/gpu.hh"
#include "psyqo/gte-kernels.hh"
#include "psyqo/gte-registers.hh"
#include "psyqo/iso9660-parser.hh"
#include "psyqo/ordering-table.hh"
#include "psyqo/primitives/common.hh"
#include "psyqo/primitives/misc.hh"
#include "psyqo/primitives/quads.hh"
#include "psyqo/primitives/triangles.hh"
#include "psyqo/scene.hh"
#include "psyqo/advancedpad.hh"
#include "psyqo/soft-math.hh"
#include "psyqo/trigonometry.hh"

#include "psyqo-paths/cdrom-loader.hh"

#include "prm.h"
#include "skm.h"
#include "col.h"
#include "trn.h"
#include "collision.h"
#include "vram_alloc.h"

// ── GPU texture window primitive (GP0 E2h) ───────────────────────────────
// Inserted before each textured triangle to enable per-pixel UV wrapping,
// matching the N64 RDP's automatic texture tiling behavior.
struct TexWindow {
    uint32_t command = 0xe2000000u;
};

// ── Tuning constants ─────────────────────────────────────────────────────

constexpr int OT_SIZE      = 1024;
constexpr int MAX_TRIS     = 1200;
constexpr int MAX_VTX      = 256;
constexpr int SCREEN_W     = 320;
constexpr int SCREEN_H     = 240;
constexpr int H_PROJ       = 180;
constexpr int NUM_SCENES   = 9;
constexpr int TOTAL_ROOMS  = 114;
constexpr int SKEL_SCALE   = 100;  // OoT Actor_SetScale(0.01) - applied at runtime

// ── Shared types ─────────────────────────────────────────────────────────

struct ScreenVtx {
    int16_t sx, sy;
    uint16_t sz;
    uint16_t pad;
};

struct BoneState {
    psyqo::Matrix33 rot;
    int32_t tx, ty, tz;
};

// ── Globals (defined in main.cpp) ────────────────────────────────────────

extern VramAlloc::Allocator g_vramAlloc;
extern ScreenVtx g_scratch[MAX_VTX];

// ── Scene / Room tables (defined in room.cpp) ───────────────────────────────

struct SpawnPoint {
    int16_t x, y, z;
    int16_t rot_y;  // OoT s16 binary angle
};

struct SceneDesc {
    const char* name;        // display name, e.g. "Deku Tree"
    const char* col_file;    // "COL/YDAN.COL;1"
    const char* trn_file;    // "TRN/YDAN.TRN;1"
    uint8_t  num_rooms;      // number of rooms in this scene
    uint8_t  room_base;      // index into ROOM_FILES[] for first room
    SpawnPoint spawn;        // default entrance spawn point
};

extern const SceneDesc SCENES[NUM_SCENES];
extern const char* const ROOM_FILES[TOTAL_ROOMS];

// ── Application ──────────────────────────────────────────────────────────

class OotApp final : public psyqo::Application {
    void prepare() override;
    void createScene() override;
  public:
    psyqo::Trig<> m_trig;
    psyqo::AdvancedPad m_pad;
    psyqo::Font<> m_font;
    psyqo::CDRomDevice m_cdrom;
    psyqo::ISO9660Parser m_isoParser{&m_cdrom};
    psyqo::paths::CDRomLoader m_loader;
};

extern OotApp app;

// ── Room renderer scene ──────────────────────────────────────────────────

class RoomScene final : public psyqo::Scene {
    void start(StartReason reason) override;
    void frame() override;

    // Player state (OoT-style movement)
    int16_t m_playerRotY = 0;    // facing direction (OoT s16 binary angle)
    int32_t m_playerSpeed = 0;   // current speed (4.12 fixed-point, OoT units)

    // Camera (orbit around player)
    psyqo::Angle m_camRotY;      // orbit yaw
    psyqo::Angle m_camRotX;      // orbit pitch
    int32_t m_camDist = 120;     // orbit radius
    int32_t m_camX = 0, m_camY = 0, m_camZ = 0;  // computed each frame

    // Double-buffered rendering resources
    psyqo::OrderingTable<OT_SIZE> m_ots[2];
    psyqo::Fragments::SimpleFragment<psyqo::Prim::FastFill> m_clear[2];
    eastl::array<psyqo::Fragments::SimpleFragment<psyqo::Prim::GouraudTexturedTriangle>, MAX_TRIS> m_tris[2];
    eastl::array<psyqo::Fragments::SimpleFragment<TexWindow>, MAX_TRIS> m_texWins[2];

    int m_triCount = 0;
    int m_parity = 0;

    // Scene / room streaming
    int m_sceneIdx = 0;              // current scene (0..NUM_SCENES-1)
    int m_localRoomIdx = 0;          // room within current scene

    psyqo::Buffer<uint8_t> m_roomBuf[2];  // double-buffered room data
    int m_activeSlot = 0;                   // which m_roomBuf[] is active
    const uint8_t* m_prm = nullptr;
    bool m_loading = false;
    bool m_needUpload = false;
    bool m_selectHeld = false;

    // Transition data (loaded per scene, stays resident)
    psyqo::Buffer<uint8_t> m_trnBuf;
    const uint8_t* m_trnData = nullptr;

    // Predictive preloading
    int  m_preloadRoom = -1;         // local room index being preloaded, or -1
    bool m_preloading = false;       // true while async preload read is in-flight
    bool m_preloadReady = false;     // true once preload read completed

    // Debug texture grid
    bool m_debugView = false;
    bool m_startHeld = false;
    eastl::array<psyqo::Fragments::SimpleFragment<psyqo::Prim::TexturedQuad>, VramAlloc::MAX_TEXTURES> m_debugQuads[2];

    // Collision state
    psyqo::Buffer<uint8_t> m_colBuf;
    const uint8_t* m_colData = nullptr;
    int m_colSceneIdx = -1;
    int32_t m_velY = 0;
    bool m_onGround = false;

    // Water state
    int32_t m_waterSurfaceY = INT32_MIN;
    bool    m_inWater = false;

    // Skeleton state
    psyqo::Buffer<uint8_t> m_skelBuf;
    const uint8_t* m_skm = nullptr;
    SKM::LimbMeshCache m_limbCache;
    bool m_skelLoaded = false;
    bool m_skelVisible = true;
    int m_skelTexBase = 0;
    int32_t m_skelX = 0, m_skelY = 0, m_skelZ = 0;  // world position

    // Animation (driven by movement state)
    int m_animIdx = 0;
    int m_animFrame = 0;

    // Input debounce
    bool m_triangleHeld = false;
    bool m_l1Held = false;
    bool m_r1Held = false;

    // Transition state
    int m_transitionCooldown = 0;

    // Bone hierarchy
    BoneState m_bones[21];

    // Room methods (room.cpp)
    void loadScene(int sceneIdx, int startRoom = 0);
    void loadRoom(int localRoomIdx);
    void loadCollision(int sceneIdx, int startRoom);
    void loadTransitions(int sceneIdx, int startRoom);
    void preloadRoom(int localRoomIdx);
    void transitionToRoom(int localRoomIdx, int16_t spawnX, int16_t spawnY, int16_t spawnZ, int16_t rot);
    void uploadTextures();
    void renderChunk(const PRM::ChunkDesc& chunk);
    void transformVertices(const PRM::Pos* pos, int count);
    void renderDebugGrid();

    // Player methods (skeleton.cpp)
    void loadSkeleton();
    void updatePlayer();
    void checkTransitions();
    void applyWallCollision();
    void applyFloorCollision();
    void applyCeilingCollision();
    void checkWater();
    void computeBones(const int16_t* frame);
    void computeBoneRecurse(int limbIdx, const BoneState& parent, const int16_t* frame);
    void renderSkeleton(const psyqo::Matrix33& renderRot, int32_t camTX, int32_t camTY, int32_t camTZ);
    void drawLimb(int limbIdx, const psyqo::Matrix33& renderRot, int32_t camTX, int32_t camTY, int32_t camTZ);
};

extern RoomScene roomScene;
