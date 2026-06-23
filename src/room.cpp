// OoT PS1 - Room loading, texture upload, chunk rendering, debug grid.

#include "scene.h"

using namespace psyqo::trig_literals;

// ── Scene table ──────────────────────────────────────────────────────────
//
// Each scene groups rooms that share collision and transition data.
// ROOM_FILES[] is a flat array; each scene's rooms start at room_base.

const SceneDesc SCENES[NUM_SCENES] = {
    // name              col_file               trn_file               rooms base  spawn
    {"Deku Tree",       "COL/YDAN.COL;1",      "TRN/YDAN.TRN;1",      12,   0,  {   -4,    0,  603, -32768}},
    {"Kokiri Forest",   "COL/SPOT04.COL;1",     "TRN/SPOT04.TRN;1",     3,  12,  {  -68,  -80,  941,  25486}},
    {"Hyrule Field",    "COL/SPOT00.COL;1",     "TRN/SPOT00.TRN;1",     1,  15,  {  160,    0, 1415,  -3641}},
    {"Forest Temple",   "COL/BMORI1.COL;1",     "TRN/BMORI1.TRN;1",    23,  16,  {  110,  309,  781, -32768}},
    {"Fire Temple",     "COL/HIDAN.COL;1",      "TRN/HIDAN.TRN;1",     27,  39,  {    5,    0,  983, -32768}},
    {"Water Temple",    "COL/MIZUSIN.COL;1",    "TRN/MIZUSIN.TRN;1",   23,  66,  { -182,  620,  969, -32768}},
    {"Shadow Temple",   "COL/HAKADAN.COL;1",    "TRN/HAKADAN.TRN;1",   23,  89,  { -254,  -63,  734, -32768}},
    {"Lon Lon Ranch",   "COL/SPOT15.COL;1",     "TRN/SPOT15.TRN;1",     1, 112,  { -225, 1086, 3743, -27307}},
    {"Kakariko",        "COL/SPOT01.COL;1",     "TRN/SPOT01.TRN;1",     1, 113,  {-2649,  138, 1063,  16384}},
};

const char* const ROOM_FILES[TOTAL_ROOMS] = {
    // Scene 0: ydan (Deku Tree) - rooms 0-11, base=0
    "ROOMS/YDAN_0.PRM;1",  "ROOMS/YDAN_1.PRM;1",  "ROOMS/YDAN_2.PRM;1",
    "ROOMS/YDAN_3.PRM;1",  "ROOMS/YDAN_4.PRM;1",  "ROOMS/YDAN_5.PRM;1",
    "ROOMS/YDAN_6.PRM;1",  "ROOMS/YDAN_7.PRM;1",  "ROOMS/YDAN_8.PRM;1",
    "ROOMS/YDAN_9.PRM;1",  "ROOMS/YDAN_10.PRM;1", "ROOMS/YDAN_11.PRM;1",
    // Scene 1: spot04 (Kokiri Forest) - rooms 0-2, base=12
    "ROOMS/SPOT04_0.PRM;1", "ROOMS/SPOT04_1.PRM;1", "ROOMS/SPOT04_2.PRM;1",
    // Scene 2: spot00 (Hyrule Field) - room 0, base=15
    "ROOMS/SPOT00_0.PRM;1",
    // Scene 3: Bmori1 (Forest Temple) - rooms 0-22, base=16
    "ROOMS/BMORI1_0.PRM;1",  "ROOMS/BMORI1_1.PRM;1",  "ROOMS/BMORI1_2.PRM;1",
    "ROOMS/BMORI1_3.PRM;1",  "ROOMS/BMORI1_4.PRM;1",  "ROOMS/BMORI1_5.PRM;1",
    "ROOMS/BMORI1_6.PRM;1",  "ROOMS/BMORI1_7.PRM;1",  "ROOMS/BMORI1_8.PRM;1",
    "ROOMS/BMORI1_9.PRM;1",  "ROOMS/BMORI1_10.PRM;1", "ROOMS/BMORI1_11.PRM;1",
    "ROOMS/BMORI1_12.PRM;1", "ROOMS/BMORI1_13.PRM;1", "ROOMS/BMORI1_14.PRM;1",
    "ROOMS/BMORI1_15.PRM;1", "ROOMS/BMORI1_16.PRM;1", "ROOMS/BMORI1_17.PRM;1",
    "ROOMS/BMORI1_18.PRM;1", "ROOMS/BMORI1_19.PRM;1", "ROOMS/BMORI1_20.PRM;1",
    "ROOMS/BMORI1_21.PRM;1", "ROOMS/BMORI1_22.PRM;1",
    // Scene 4: HIDAN (Fire Temple) - rooms 0-26, base=39
    "ROOMS/HIDAN_0.PRM;1",  "ROOMS/HIDAN_1.PRM;1",  "ROOMS/HIDAN_2.PRM;1",
    "ROOMS/HIDAN_3.PRM;1",  "ROOMS/HIDAN_4.PRM;1",  "ROOMS/HIDAN_5.PRM;1",
    "ROOMS/HIDAN_6.PRM;1",  "ROOMS/HIDAN_7.PRM;1",  "ROOMS/HIDAN_8.PRM;1",
    "ROOMS/HIDAN_9.PRM;1",  "ROOMS/HIDAN_10.PRM;1", "ROOMS/HIDAN_11.PRM;1",
    "ROOMS/HIDAN_12.PRM;1", "ROOMS/HIDAN_13.PRM;1", "ROOMS/HIDAN_14.PRM;1",
    "ROOMS/HIDAN_15.PRM;1", "ROOMS/HIDAN_16.PRM;1", "ROOMS/HIDAN_17.PRM;1",
    "ROOMS/HIDAN_18.PRM;1", "ROOMS/HIDAN_19.PRM;1", "ROOMS/HIDAN_20.PRM;1",
    "ROOMS/HIDAN_21.PRM;1", "ROOMS/HIDAN_22.PRM;1", "ROOMS/HIDAN_23.PRM;1",
    "ROOMS/HIDAN_24.PRM;1", "ROOMS/HIDAN_25.PRM;1", "ROOMS/HIDAN_26.PRM;1",
    // Scene 5: MIZUsin (Water Temple) - rooms 0-22, base=66
    "ROOMS/MIZUSIN_0.PRM;1",  "ROOMS/MIZUSIN_1.PRM;1",  "ROOMS/MIZUSIN_2.PRM;1",
    "ROOMS/MIZUSIN_3.PRM;1",  "ROOMS/MIZUSIN_4.PRM;1",  "ROOMS/MIZUSIN_5.PRM;1",
    "ROOMS/MIZUSIN_6.PRM;1",  "ROOMS/MIZUSIN_7.PRM;1",  "ROOMS/MIZUSIN_8.PRM;1",
    "ROOMS/MIZUSIN_9.PRM;1",  "ROOMS/MIZUSIN_10.PRM;1", "ROOMS/MIZUSIN_11.PRM;1",
    "ROOMS/MIZUSIN_12.PRM;1", "ROOMS/MIZUSIN_13.PRM;1", "ROOMS/MIZUSIN_14.PRM;1",
    "ROOMS/MIZUSIN_15.PRM;1", "ROOMS/MIZUSIN_16.PRM;1", "ROOMS/MIZUSIN_17.PRM;1",
    "ROOMS/MIZUSIN_18.PRM;1", "ROOMS/MIZUSIN_19.PRM;1", "ROOMS/MIZUSIN_20.PRM;1",
    "ROOMS/MIZUSIN_21.PRM;1", "ROOMS/MIZUSIN_22.PRM;1",
    // Scene 6: HAKAdan (Shadow Temple) - rooms 0-22, base=89
    "ROOMS/HAKADAN_0.PRM;1",  "ROOMS/HAKADAN_1.PRM;1",  "ROOMS/HAKADAN_2.PRM;1",
    "ROOMS/HAKADAN_3.PRM;1",  "ROOMS/HAKADAN_4.PRM;1",  "ROOMS/HAKADAN_5.PRM;1",
    "ROOMS/HAKADAN_6.PRM;1",  "ROOMS/HAKADAN_7.PRM;1",  "ROOMS/HAKADAN_8.PRM;1",
    "ROOMS/HAKADAN_9.PRM;1",  "ROOMS/HAKADAN_10.PRM;1", "ROOMS/HAKADAN_11.PRM;1",
    "ROOMS/HAKADAN_12.PRM;1", "ROOMS/HAKADAN_13.PRM;1", "ROOMS/HAKADAN_14.PRM;1",
    "ROOMS/HAKADAN_15.PRM;1", "ROOMS/HAKADAN_16.PRM;1", "ROOMS/HAKADAN_17.PRM;1",
    "ROOMS/HAKADAN_18.PRM;1", "ROOMS/HAKADAN_19.PRM;1", "ROOMS/HAKADAN_20.PRM;1",
    "ROOMS/HAKADAN_21.PRM;1", "ROOMS/HAKADAN_22.PRM;1",
    // Scene 7: spot15 (Lon Lon Ranch) - room 0, base=112
    "ROOMS/SPOT15_0.PRM;1",
    // Scene 8: spot01 (Kakariko) - room 0, base=113
    "ROOMS/SPOT01_0.PRM;1",
};

// ── Scene loading (entry point for entering a scene) ────────────────────

void RoomScene::loadScene(int sceneIdx, int startRoom) {
    m_sceneIdx = sceneIdx;
    const auto& scene = SCENES[sceneIdx];

    // Set spawn point and reset player state
    m_skelX = scene.spawn.x;
    m_skelY = scene.spawn.y;
    m_skelZ = scene.spawn.z;
    m_playerRotY = scene.spawn.rot_y;
    m_playerSpeed = 0;
    m_velY = 0;
    m_onGround = false;
    m_waterSurfaceY = INT32_MIN;
    m_inWater = false;
    m_animIdx = 0;
    m_animFrame = 0;
    m_camRotY = 1.0_pi;
    m_camRotX = psyqo::Angle(0.05_pi);
    m_camDist = 120;

    // Clear preload state
    m_preloading = false;
    m_preloadReady = false;
    m_preloadRoom = -1;

    // Clear transition cooldown so new scene can trigger immediately
    m_transitionCooldown = 0;

    // Loading chain: collision → transitions → room
    loadCollision(sceneIdx, startRoom);
}

// ── Room loading via CD-ROM ──────────────────────────────────────────────

void RoomScene::loadRoom(int localRoomIdx) {
    const auto& scene = SCENES[m_sceneIdx];
    int globalIdx = scene.room_base + localRoomIdx;

    m_loading = true;
    m_prm = nullptr;
    m_localRoomIdx = localRoomIdx;

    // Cancel any pending preload
    m_preloading = false;
    m_preloadReady = false;
    m_preloadRoom = -1;

    app.m_loader.readFile(ROOM_FILES[globalIdx], app.m_isoParser,
        [this](psyqo::Buffer<uint8_t>&& buffer) {
            m_roomBuf[m_activeSlot] = eastl::move(buffer);
            m_prm = m_roomBuf[m_activeSlot].data();
            m_needUpload = (m_prm != nullptr);
            m_loading = false;
        });
}

// ── Collision loading via CD-ROM ─────────────────────────────────────────

void RoomScene::loadCollision(int sceneIdx, int startRoom) {
    // Skip if this scene's collision is already loaded
    if (sceneIdx == m_colSceneIdx && m_colData != nullptr) {
        loadTransitions(sceneIdx, startRoom);
        return;
    }

    m_colData = nullptr;

    app.m_loader.readFile(SCENES[sceneIdx].col_file, app.m_isoParser,
        [this, sceneIdx, startRoom](psyqo::Buffer<uint8_t>&& buffer) {
            m_colBuf = eastl::move(buffer);
            if (m_colBuf.size() >= sizeof(COL::Header)) {
                const auto* hdr = COL::header(m_colBuf.data());
                if (hdr->magic[0] == 'C' && hdr->magic[1] == 'O' &&
                    hdr->magic[2] == 'L' && hdr->magic[3] == 0x01) {
                    m_colData = m_colBuf.data();
                    m_colSceneIdx = sceneIdx;
                }
            }
            loadTransitions(sceneIdx, startRoom);
        });
}

// ── Transition data loading via CD-ROM ──────────────────────────────────

void RoomScene::loadTransitions(int sceneIdx, int startRoom) {
    m_trnData = nullptr;

    app.m_loader.readFile(SCENES[sceneIdx].trn_file, app.m_isoParser,
        [this, startRoom](psyqo::Buffer<uint8_t>&& buffer) {
            m_trnBuf = eastl::move(buffer);
            if (m_trnBuf.size() >= sizeof(TRN::Header)) {
                const auto* hdr = TRN::header(m_trnBuf.data());
                if (hdr->magic[0] == 'T' && hdr->magic[1] == 'R' &&
                    hdr->magic[2] == 'N' && hdr->magic[3] == 0x01) {
                    m_trnData = m_trnBuf.data();
                }
            }
            loadRoom(startRoom);
        });
}

// ── Predictive preloading ───────────────────────────────────────────────

void RoomScene::preloadRoom(int localRoomIdx) {
    // Only preload if not currently loading or preloading
    if (m_loading || m_preloading) return;
    // Don't preload the room we're already in
    if (localRoomIdx == m_localRoomIdx) return;
    // Don't re-preload if already done for this room
    if (localRoomIdx == m_preloadRoom && m_preloadReady) return;

    const auto& scene = SCENES[m_sceneIdx];
    int globalIdx = scene.room_base + localRoomIdx;
    int inactiveSlot = 1 - m_activeSlot;

    m_preloadRoom = localRoomIdx;
    m_preloading = true;
    m_preloadReady = false;

    app.m_loader.readFile(ROOM_FILES[globalIdx], app.m_isoParser,
        [this, inactiveSlot](psyqo::Buffer<uint8_t>&& buffer) {
            m_roomBuf[inactiveSlot] = eastl::move(buffer);
            m_preloading = false;
            m_preloadReady = (m_roomBuf[inactiveSlot].size() > 0);
        });
}

void RoomScene::transitionToRoom(int localRoomIdx,
                                  int16_t spawnX, int16_t spawnY,
                                  int16_t spawnZ, int16_t rot) {
    // Reposition player at transition destination
    m_skelX = spawnX;
    m_skelY = spawnY;
    m_skelZ = spawnZ;
    m_playerRotY = rot;
    m_playerSpeed = 0;
    m_velY = 0;
    m_onGround = false;

    if (m_preloadReady && m_preloadRoom == localRoomIdx) {
        // Fast path: preloaded data is ready - swap buffer slots
        m_activeSlot = 1 - m_activeSlot;
        m_prm = m_roomBuf[m_activeSlot].data();
        m_localRoomIdx = localRoomIdx;
        m_needUpload = true;  // must re-upload textures to VRAM
        m_preloadReady = false;
        m_preloadRoom = -1;
    } else {
        // Slow path: blocking load with "Loading..." text
        m_preloadReady = false;
        m_preloadRoom = -1;
        m_preloading = false;
        loadRoom(localRoomIdx);
    }
}

// ── Upload textures to VRAM (room + skeleton) ────────────────────────────

void RoomScene::uploadTextures() {
    g_vramAlloc.reset();

    // Upload room textures
    if (m_prm) {
        const auto* hdr = PRM::header(m_prm);
        const auto* descs = PRM::texDescs(m_prm);
        const uint8_t* tdata = PRM::texData(m_prm);

        for (int i = 0; i < hdr->num_textures; i++) {
            const auto& td = descs[i];
            uint16_t clut_n = PRM::texClutCount(td);

            int slot = g_vramAlloc.alloc(td.width, td.height, td.format, clut_n);
            if (slot < 0) continue;

            const uint16_t* pix = reinterpret_cast<const uint16_t*>(
                tdata + td.data_offset);
            gpu().uploadToVRAM(pix, g_vramAlloc.pixelRect(slot));

            uint32_t pix_bytes = PRM::texPixelSize(td);
            pix_bytes = (pix_bytes + 1) & ~1u;
            const uint16_t* clut = reinterpret_cast<const uint16_t*>(
                tdata + td.data_offset + pix_bytes);
            gpu().uploadToVRAM(clut, g_vramAlloc.clutRect(slot));
        }
    }

    // Upload skeleton textures (appended after room textures)
    m_skelTexBase = g_vramAlloc.numSlots();
    if (m_skm) {
        const auto* shdr = SKM::header(m_skm);
        const auto* sdescs = SKM::texDescs(m_skm);
        const uint8_t* stdata = SKM::texData(m_skm);

        for (int i = 0; i < shdr->num_textures; i++) {
            const auto& td = sdescs[i];
            uint16_t clut_n = SKM::texClutCount(td);

            int slot = g_vramAlloc.alloc(td.width, td.height, td.format, clut_n);
            if (slot < 0) continue;

            const uint16_t* pix = reinterpret_cast<const uint16_t*>(
                stdata + td.data_offset);
            gpu().uploadToVRAM(pix, g_vramAlloc.pixelRect(slot));

            uint32_t pix_bytes = SKM::texPixelSize(td);
            pix_bytes = (pix_bytes + 1) & ~1u;
            const uint16_t* clut = reinterpret_cast<const uint16_t*>(
                stdata + td.data_offset + pix_bytes);
            gpu().uploadToVRAM(clut, g_vramAlloc.clutRect(slot));
        }
    }
}

// ── Batch vertex transform ───────────────────────────────────────────────

void RoomScene::transformVertices(const PRM::Pos* pos, int count) {
    int i = 0;

    for (; i + 2 < count; i += 3) {
        const auto* r0 = reinterpret_cast<const uint32_t*>(&pos[i]);
        const auto* r1 = reinterpret_cast<const uint32_t*>(&pos[i + 1]);
        const auto* r2 = reinterpret_cast<const uint32_t*>(&pos[i + 2]);

        psyqo::GTE::write<psyqo::GTE::Register::VXY0, psyqo::GTE::Unsafe>(r0[0]);
        psyqo::GTE::write<psyqo::GTE::Register::VZ0, psyqo::GTE::Unsafe>(r0[1]);
        psyqo::GTE::write<psyqo::GTE::Register::VXY1, psyqo::GTE::Unsafe>(r1[0]);
        psyqo::GTE::write<psyqo::GTE::Register::VZ1, psyqo::GTE::Unsafe>(r1[1]);
        psyqo::GTE::write<psyqo::GTE::Register::VXY2, psyqo::GTE::Unsafe>(r2[0]);
        psyqo::GTE::write<psyqo::GTE::Register::VZ2, psyqo::GTE::Safe>(r2[1]);
        psyqo::GTE::Kernels::rtpt();

        psyqo::GTE::read<psyqo::GTE::Register::SXY0>(
            reinterpret_cast<uint32_t*>(&g_scratch[i].sx));
        psyqo::GTE::read<psyqo::GTE::Register::SXY1>(
            reinterpret_cast<uint32_t*>(&g_scratch[i + 1].sx));
        psyqo::GTE::read<psyqo::GTE::Register::SXY2>(
            reinterpret_cast<uint32_t*>(&g_scratch[i + 2].sx));

        uint32_t sz1, sz2, sz3;
        psyqo::GTE::read<psyqo::GTE::Register::SZ1>(&sz1);
        psyqo::GTE::read<psyqo::GTE::Register::SZ2>(&sz2);
        psyqo::GTE::read<psyqo::GTE::Register::SZ3>(&sz3);
        g_scratch[i].sz     = static_cast<uint16_t>(sz1);
        g_scratch[i + 1].sz = static_cast<uint16_t>(sz2);
        g_scratch[i + 2].sz = static_cast<uint16_t>(sz3);
    }

    for (; i < count; i++) {
        const auto* r = reinterpret_cast<const uint32_t*>(&pos[i]);
        psyqo::GTE::write<psyqo::GTE::Register::VXY0, psyqo::GTE::Unsafe>(r[0]);
        psyqo::GTE::write<psyqo::GTE::Register::VZ0, psyqo::GTE::Safe>(r[1]);
        psyqo::GTE::Kernels::rtps();

        psyqo::GTE::read<psyqo::GTE::Register::SXY2>(
            reinterpret_cast<uint32_t*>(&g_scratch[i].sx));
        uint32_t sz;
        psyqo::GTE::read<psyqo::GTE::Register::SZ3>(&sz);
        g_scratch[i].sz = static_cast<uint16_t>(sz);
    }
}

// ── Render one chunk ─────────────────────────────────────────────────────

void RoomScene::renderChunk(const PRM::ChunkDesc& chunk) {
    if (chunk.num_verts == 0 || chunk.num_tris == 0) return;

    const auto* pos = PRM::positions(m_prm, chunk);
    const auto* col = PRM::colors(m_prm, chunk);
    const auto* uv = PRM::uvs(m_prm, chunk);
    const auto* tri = PRM::triangles(m_prm, chunk);

    transformVertices(pos, chunk.num_verts);

    for (int t = 0; t < chunk.num_tris && m_triCount < MAX_TRIS; t++) {
        const auto& idx = tri[t];
        const auto& sv0 = g_scratch[idx.v0];
        const auto& sv1 = g_scratch[idx.v1];
        const auto& sv2 = g_scratch[idx.v2];

        if (sv0.sz == 0 || sv1.sz == 0 || sv2.sz == 0) continue;

        int32_t dx0 = sv1.sx - sv0.sx;
        int32_t dy0 = sv1.sy - sv0.sy;
        int32_t dx1 = sv2.sx - sv0.sx;
        int32_t dy1 = sv2.sy - sv0.sy;
        int32_t cross = dx0 * dy1 - dx1 * dy0;
        if (cross >= 0) continue;

        // Reject only when all vertices are off the same edge (bbox rejection)
        if (sv0.sx < -512 && sv1.sx < -512 && sv2.sx < -512) continue;
        if (sv0.sx >  512 && sv1.sx >  512 && sv2.sx >  512) continue;
        if (sv0.sy < -512 && sv1.sy < -512 && sv2.sy < -512) continue;
        if (sv0.sy >  512 && sv1.sy >  512 && sv2.sy >  512) continue;

        uint32_t maxZ = sv0.sz;
        if (sv1.sz > maxZ) maxZ = sv1.sz;
        if (sv2.sz > maxZ) maxZ = sv2.sz;
        int32_t otIdx = static_cast<int32_t>((maxZ * OT_SIZE) >> 12);
        if (otIdx <= 0 || otIdx >= OT_SIZE) continue;

        auto& frag = m_tris[m_parity][m_triCount];
        auto& p = frag.primitive;

        p.pointA.x = sv0.sx; p.pointA.y = sv0.sy;
        p.pointB.x = sv1.sx; p.pointB.y = sv1.sy;
        p.pointC.x = sv2.sx; p.pointC.y = sv2.sy;

        // N64 vertex colors are 0-255 (255 = full bright). PS1 gouraud
        // modulates tex × color / 128 (128 = 1.0). Halve to match.
        const auto& c0 = col[idx.v0];
        const auto& c1 = col[idx.v1];
        const auto& c2 = col[idx.v2];
        p.setColorA(psyqo::Color{{.r = uint8_t(c0.r >> 1),
                                   .g = uint8_t(c0.g >> 1),
                                   .b = uint8_t(c0.b >> 1)}});
        p.setColorB(psyqo::Color{{.r = uint8_t(c1.r >> 1),
                                   .g = uint8_t(c1.g >> 1),
                                   .b = uint8_t(c1.b >> 1)}});
        p.setColorC(psyqo::Color{{.r = uint8_t(c2.r >> 1),
                                   .g = uint8_t(c2.g >> 1),
                                   .b = uint8_t(c2.b >> 1)}});

        if (idx.tex_id >= g_vramAlloc.numSlots()) continue;
        const auto& ti = g_vramAlloc.info(idx.tex_id);
        // Raw UVs + TPage offset - texture window handles per-pixel wrapping
        p.uvA.u = uv[idx.v0].u + ti.u_off;
        p.uvA.v = uv[idx.v0].v + ti.v_off;
        p.uvB.u = uv[idx.v1].u + ti.u_off;
        p.uvB.v = uv[idx.v1].v + ti.v_off;
        p.uvC.u = uv[idx.v2].u + ti.u_off;
        p.uvC.v = uv[idx.v2].v + ti.v_off;

        p.tpage = ti.tpage;
        p.clutIndex = ti.clut;

        // Insert texture window command THEN triangle (OT is LIFO per slot)
        m_ots[m_parity].insert(frag, otIdx);
        auto& tw = m_texWins[m_parity][m_triCount];
        tw.primitive.command = ti.tex_window;
        m_ots[m_parity].insert(tw, otIdx);
        m_triCount++;
    }
}

// ── Debug texture grid ───────────────────────────────────────────────────

void RoomScene::renderDebugGrid() {
    gpu().waitChainIdle();
    m_parity = gpu().getParity();
    auto& ot = m_ots[m_parity];
    ot.clear();

    if (m_prm) {
        const auto* hdr = PRM::header(m_prm);
        const auto* descs = PRM::texDescs(m_prm);
        int numTex = hdr->num_textures;
        if (numTex > VramAlloc::MAX_TEXTURES) numTex = VramAlloc::MAX_TEXTURES;

        constexpr int COLS = 8;
        constexpr int CELL_W = 40;
        constexpr int CELL_H = 52;
        constexpr int QUAD_SZ = 36;
        constexpr int16_t TOP_Y = 20;

        for (int i = 0; i < numTex; i++) {
            int col = i % COLS;
            int row = i / COLS;
            int16_t cx = static_cast<int16_t>(col * CELL_W + (CELL_W - QUAD_SZ) / 2);
            int16_t cy = static_cast<int16_t>(TOP_Y + row * CELL_H);

            const auto& td = descs[i];
            const auto& ti = g_vramAlloc.info(i);

            auto& frag = m_debugQuads[m_parity][i];
            auto& q = frag.primitive;
            q.setColor(psyqo::Color{{.r = 128, .g = 128, .b = 128}});

            q.pointA.x = cx;            q.pointA.y = cy;
            q.pointB.x = cx + QUAD_SZ;  q.pointB.y = cy;
            q.pointC.x = cx;            q.pointC.y = cy + QUAD_SZ;
            q.pointD.x = cx + QUAD_SZ;  q.pointD.y = cy + QUAD_SZ;

            uint8_t maxU = static_cast<uint8_t>(td.width > QUAD_SZ ? QUAD_SZ - 1 : td.width - 1);
            uint8_t maxV = static_cast<uint8_t>(td.height > QUAD_SZ ? QUAD_SZ - 1 : td.height - 1);
            q.uvA.u = ti.u_off;          q.uvA.v = ti.v_off;
            q.uvB.u = ti.u_off + maxU;   q.uvB.v = ti.v_off;
            q.uvC.u = ti.u_off;          q.uvC.v = ti.v_off + maxV;
            q.uvD.u = ti.u_off + maxU;   q.uvD.v = ti.v_off + maxV;

            q.tpage = ti.tpage;
            q.clutIndex = ti.clut;

            ot.insert(frag, 1);
        }
    }

    auto& clear = m_clear[m_parity];
    psyqo::Color bg{{.r = 0x10, .g = 0x10, .b = 0x10}};
    gpu().getNextClear(clear.primitive, bg);
    gpu().chain(clear);
    gpu().chain(ot);

    psyqo::Color white{{.r = 255, .g = 255, .b = 255}};
    if (m_prm) {
        const auto* hdr = PRM::header(m_prm);
        app.m_font.printf(gpu(), {{.x = 4, .y = 4}}, white,
            "%s R%d/%d TEX:%d", SCENES[m_sceneIdx].name,
            m_localRoomIdx, SCENES[m_sceneIdx].num_rooms,
            hdr->num_textures);

        const auto* descs = PRM::texDescs(m_prm);
        int numTex = hdr->num_textures;
        if (numTex > VramAlloc::MAX_TEXTURES) numTex = VramAlloc::MAX_TEXTURES;
        constexpr int COLS = 8;
        constexpr int CELL_W = 40;
        constexpr int CELL_H = 52;
        constexpr int QUAD_SZ = 36;
        constexpr int16_t TOP_Y = 20;

        psyqo::Color gray{{.r = 160, .g = 160, .b = 160}};
        for (int i = 0; i < numTex; i++) {
            int col = i % COLS;
            int row = i / COLS;
            int16_t lx = static_cast<int16_t>(col * CELL_W + 2);
            int16_t ly = static_cast<int16_t>(TOP_Y + row * CELL_H + QUAD_SZ + 2);
            const auto& td = descs[i];
            app.m_font.printf(gpu(), {{.x = lx, .y = ly}}, gray,
                "%d %dx%d", i, td.width, td.height);
        }
    } else {
        app.m_font.printf(gpu(), {{.x = 4, .y = 4}}, white, "No room data");
    }
}
