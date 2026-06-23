// Zelda: Ocarina of Time - PS1 Port
// CD-ROM streaming room renderer with gouraud-textured triangles.
// Skeletal mesh overlay with hierarchical bone transforms.
//
// Rooms are loaded from disc on demand via CDRomDevice + ISO9660Parser.
// Only one room is resident in RAM at a time. Select button cycles rooms.
// Link's skeleton renders as an overlay at world origin with animation.
//
// Render pipeline per chunk/limb:
//   1. Batch-transform ALL vertices via GTE RTPT (3 at a time)
//   2. For each triangle:
//      a. Software NCLIP (cross product on pre-transformed screen coords)
//      b. Average Z → ordering table index
//      c. Per-vertex UVs + texture lookup
//      d. Insert into ordering table

#include "scene.h"

using namespace psyqo::trig_literals;
using namespace psyqo::fixed_point_literals;

// ── Global instances ─────────────────────────────────────────────────────

OotApp app;
RoomScene roomScene;
VramAlloc::Allocator g_vramAlloc;
ScreenVtx g_scratch[MAX_VTX];

// ── Application setup ────────────────────────────────────────────────────

void OotApp::prepare() {
    psyqo::GPU::Configuration config;
    config.set(psyqo::GPU::Resolution::W320)
        .set(psyqo::GPU::VideoMode::AUTO)
        .set(psyqo::GPU::ColorMode::C15BITS)
        .set(psyqo::GPU::Interlace::PROGRESSIVE);
    gpu().initialize(config);
    m_cdrom.prepare();
}

void OotApp::createScene() {
    m_pad.initialize();
    m_font.uploadSystemFont(gpu());
    pushScene(&roomScene);
}

// ── Scene start ──────────────────────────────────────────────────────────

void RoomScene::start(StartReason) {
    psyqo::GTE::write<psyqo::GTE::Register::OFX, psyqo::GTE::Unsafe>(
        psyqo::FixedPoint<16>(SCREEN_W / 2.0).raw());
    psyqo::GTE::write<psyqo::GTE::Register::OFY, psyqo::GTE::Unsafe>(
        psyqo::FixedPoint<16>(SCREEN_H / 2.0).raw());
    psyqo::GTE::write<psyqo::GTE::Register::H, psyqo::GTE::Unsafe>(H_PROJ);
    psyqo::GTE::write<psyqo::GTE::Register::ZSF3, psyqo::GTE::Unsafe>(OT_SIZE / 3);
    psyqo::GTE::write<psyqo::GTE::Register::ZSF4, psyqo::GTE::Unsafe>(OT_SIZE / 4);

    loadSkeleton();
}

// ── Frame rendering ──────────────────────────────────────────────────────

void RoomScene::frame() {
    gpu().waitChainIdle();

    if (m_needUpload) {
        uploadTextures();
        m_needUpload = false;
    }

    // ── Input ────────────────────────────────────────────────────────────
    using APad = psyqo::AdvancedPad;
    constexpr auto P1 = APad::Pad::Pad1a;
    constexpr int32_t CAM_TARGET_Y = 40;   // look-at height above skeleton root

    // Scene cycling: Select button (debounced) - debug
    bool selectNow = app.m_pad.isButtonPressed(P1, APad::Select);
    if (selectNow && !m_selectHeld && !m_loading) {
        int next = (m_sceneIdx + 1) % NUM_SCENES;
        loadScene(next, 0);
    }
    m_selectHeld = selectNow;

    // Room cycling within scene: L1/R1 (debounced) - debug
    // Teleports player to a transition point connected to the target room,
    // so they don't end up in the void when rooms are in different world areas.
    bool l1Now = app.m_pad.isButtonPressed(P1, APad::L1);
    bool r1Now = app.m_pad.isButtonPressed(P1, APad::R1);
    auto debugJumpToRoom = [this](int targetRoom) {
        if (m_trnData) {
            const auto* thdr = TRN::header(m_trnData);
            const auto* ents = TRN::entries(m_trnData);
            for (int i = 0; i < thdr->num_transitions; i++) {
                const auto& e = ents[i];
                if (e.front_room == targetRoom || e.back_room == targetRoom) {
                    transitionToRoom(targetRoom, e.pos_x, e.pos_y, e.pos_z, e.rot_y);
                    return;
                }
            }
        }
        // No transition found - fall back to scene spawn
        m_skelX = SCENES[m_sceneIdx].spawn.x;
        m_skelY = SCENES[m_sceneIdx].spawn.y;
        m_skelZ = SCENES[m_sceneIdx].spawn.z;
        m_playerSpeed = 0;
        m_velY = 0;
        loadRoom(targetRoom);
    };
    if (r1Now && !m_r1Held && !m_loading) {
        int next = (m_localRoomIdx + 1) % SCENES[m_sceneIdx].num_rooms;
        debugJumpToRoom(next);
    }
    m_r1Held = r1Now;
    if (l1Now && !m_l1Held && !m_loading) {
        int prev = m_localRoomIdx - 1;
        if (prev < 0) prev = SCENES[m_sceneIdx].num_rooms - 1;
        debugJumpToRoom(prev);
    }
    m_l1Held = l1Now;

    // Debug view toggle: Start button (debounced)
    bool startNow = app.m_pad.isButtonPressed(P1, APad::Start);
    if (startNow && !m_startHeld) m_debugView = !m_debugView;
    m_startHeld = startNow;

    if (m_debugView) { renderDebugGrid(); return; }

    // Skeleton toggle: Triangle (debounced)
    bool triNow = app.m_pad.isButtonPressed(P1, APad::Triangle);
    if (triNow && !m_triangleHeld) m_skelVisible = !m_skelVisible;
    m_triangleHeld = triNow;

    // Player movement from left analog stick (handled in skeleton.cpp)
    if (m_skelVisible && m_skelLoaded) {
        updatePlayer();
    }

    // Camera orbit: right analog stick
    int32_t rsX = app.m_pad.getAdc(P1, 0) - 0x80;
    int32_t rsY = app.m_pad.getAdc(P1, 1) - 0x80;
    constexpr int32_t CAM_DEAD = 16;
    if (rsX > CAM_DEAD || rsX < -CAM_DEAD)
        m_camRotY -= psyqo::Angle((rsX * 41) >> 7, psyqo::Angle::RAW);
    if (m_camRotY < 0.0_pi)  m_camRotY += 2.0_pi;
    if (m_camRotY >= 2.0_pi) m_camRotY -= 2.0_pi;

    if (rsY > CAM_DEAD || rsY < -CAM_DEAD)
        m_camRotX += psyqo::Angle((rsY * 20) >> 7, psyqo::Angle::RAW);
    if (m_camRotX < -0.05_pi) m_camRotX = psyqo::Angle(-0.05_pi);
    if (m_camRotX > 0.35_pi)  m_camRotX = psyqo::Angle(0.35_pi);

    // ── View matrix ──────────────────────────────────────────────────────
    auto rotY = psyqo::SoftMath::generateRotationMatrix33(
        m_camRotY, psyqo::SoftMath::Axis::Y, app.m_trig);
    auto rotX = psyqo::SoftMath::generateRotationMatrix33(
        m_camRotX, psyqo::SoftMath::Axis::X, app.m_trig);
    psyqo::Matrix33 viewRot;
    psyqo::SoftMath::multiplyMatrix33(rotY, rotX, &viewRot);

    // Orbit camera: position = target - forward * distance
    // Forward = row 2 of viewRot (camera Z axis in world space)
    int32_t targetX = m_skelX;
    int32_t targetY = m_skelY + CAM_TARGET_Y;
    int32_t targetZ = m_skelZ;

    m_camX = targetX - ((viewRot.vs[2].x.raw() * m_camDist) >> 12);
    m_camY = targetY - ((viewRot.vs[2].y.raw() * m_camDist) >> 12);
    m_camZ = targetZ - ((viewRot.vs[2].z.raw() * m_camDist) >> 12);

    // Negate X and Y rows: converts OoT coords (X-right, Y-up, Z-backward)
    // to PS1 GTE screen coords (X-right, Y-down, Z-forward).
    // Y negation: flips vertical (Y-up → Y-down).
    // X negation: cancels horizontal mirror from the π camera orbit rotation
    //             (which flips both X and Z; Z flip is desired, X flip is not).
    psyqo::Matrix33 renderRot = viewRot;
    renderRot.vs[0].x = -renderRot.vs[0].x;
    renderRot.vs[0].y = -renderRot.vs[0].y;
    renderRot.vs[0].z = -renderRot.vs[0].z;
    renderRot.vs[1].x = -renderRot.vs[1].x;
    renderRot.vs[1].y = -renderRot.vs[1].y;
    renderRot.vs[1].z = -renderRot.vs[1].z;

    // Translation = -renderRot * camPos
    int32_t tx = -((renderRot.vs[0].x.raw() * m_camX +
                    renderRot.vs[0].y.raw() * m_camY +
                    renderRot.vs[0].z.raw() * m_camZ) >> 12);
    int32_t ty = -((renderRot.vs[1].x.raw() * m_camX +
                    renderRot.vs[1].y.raw() * m_camY +
                    renderRot.vs[1].z.raw() * m_camZ) >> 12);
    int32_t tz = -((renderRot.vs[2].x.raw() * m_camX +
                    renderRot.vs[2].y.raw() * m_camY +
                    renderRot.vs[2].z.raw() * m_camZ) >> 12);

    // Write camera view matrix to GTE (used by room rendering)
    psyqo::GTE::writeUnsafe<psyqo::GTE::PseudoRegister::Rotation>(renderRot);
    psyqo::GTE::write<psyqo::GTE::Register::TRX, psyqo::GTE::Unsafe>(
        static_cast<uint32_t>(tx));
    psyqo::GTE::write<psyqo::GTE::Register::TRY, psyqo::GTE::Unsafe>(
        static_cast<uint32_t>(ty));
    psyqo::GTE::write<psyqo::GTE::Register::TRZ, psyqo::GTE::Unsafe>(
        static_cast<uint32_t>(tz));

    // Reset per-frame state
    m_parity = gpu().getParity();
    m_triCount = 0;
    auto& ot = m_ots[m_parity];
    ot.clear();

    // Render room chunks (GTE has camera matrix)
    if (!m_loading && m_prm) {
        const auto* hdr = PRM::header(m_prm);
        const auto* cdescs = PRM::chunks(m_prm);
        for (int ci = 0; ci < hdr->num_chunks; ci++) {
            renderChunk(cdescs[ci]);
        }
    }

    // Render skeleton overlay (reloads GTE per limb)
    if (m_skelVisible && m_skelLoaded) {
        renderSkeleton(renderRot, tx, ty, tz);
    }

    // Submit: clear screen + ordered geometry
    auto& clear = m_clear[m_parity];
    psyqo::Color bg{{.r = 0x08, .g = 0x06, .b = 0x12}};
    gpu().getNextClear(clear.primitive, bg);
    gpu().chain(clear);
    gpu().chain(ot);

    // Debug HUD
    psyqo::Color white{{.r = 255, .g = 255, .b = 255}};
    if (m_loading) {
        app.m_font.printf(gpu(), {{.x = 8, .y = 8}}, white,
            "Loading %s R%d...", SCENES[m_sceneIdx].name, m_localRoomIdx);
    } else if (m_prm) {
        const auto* hdr = PRM::header(m_prm);
        app.m_font.printf(gpu(), {{.x = 8, .y = 8}}, white,
            "%s R%d/%d %dv %dt", SCENES[m_sceneIdx].name,
            m_localRoomIdx, SCENES[m_sceneIdx].num_rooms,
            hdr->num_verts, hdr->num_tris);
    } else {
        app.m_font.printf(gpu(), {{.x = 8, .y = 8}}, white,
            "No room data (%s)", SCENES[m_sceneIdx].name);
    }

    // Player HUD
    if (m_skelVisible && m_skelLoaded) {
        psyqo::Color cyan{{.r = 100, .g = 255, .b = 255}};
        app.m_font.printf(gpu(), {{.x = 8, .y = SCREEN_H - 16}}, cyan,
            "pos:%d,%d,%d spd:%d", m_skelX, m_skelY, m_skelZ, m_playerSpeed);

        // Nearest transition debug
        if (m_trnData) {
            const auto* thdr = TRN::header(m_trnData);
            const auto* ents = TRN::entries(m_trnData);
            int32_t bestDist = 99999;
            int bestTarget = -1;
            for (int i = 0; i < thdr->num_transitions; i++) {
                const auto& e = ents[i];
                bool conn = (e.front_room == m_localRoomIdx) ||
                            (e.back_room == m_localRoomIdx);
                if (!conn) continue;
                int target = (e.front_room == m_localRoomIdx) ?
                              e.back_room : e.front_room;
                int32_t dx = m_skelX - e.pos_x;
                int32_t dz = m_skelZ - e.pos_z;
                int32_t adx = dx < 0 ? -dx : dx;
                int32_t adz = dz < 0 ? -dz : dz;
                int32_t d = (adx > adz) ? adx + (adz >> 1) : adz + (adx >> 1);
                int32_t dy = m_skelY - e.pos_y;
                int32_t ady = dy < 0 ? -dy : dy;
                if (d < bestDist) { bestDist = d; bestTarget = target; }
            }
            psyqo::Color yellow{{.r = 255, .g = 255, .b = 100}};
            if (bestTarget >= 0) {
                app.m_font.printf(gpu(), {{.x = 8, .y = SCREEN_H - 28}}, yellow,
                    "trn:R%d d=%d", bestTarget, bestDist);
            }
        }
    }
}

int main() { return app.run(); }
