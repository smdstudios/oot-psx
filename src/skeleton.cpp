// OoT PS1 - Skeleton loading, bone hierarchy, limb rendering.

#include "scene.h"

// ── OoT angle conversion ─────────────────────────────────────────────────

// OoT s16 binary angle (0x10000 = full circle) → psyqo::Angle (FixedPoint<10>)
// Full circle: OoT = 65536, psyqo = 2048. Ratio = 32.
static inline psyqo::Angle ootAngle(int16_t raw) {
    return psyqo::Angle(static_cast<int32_t>(raw) / 32, psyqo::Angle::RAW);
}

// Build ZYX Euler rotation matrix matching OoT's Matrix_TranslateRotateZYX
static psyqo::Matrix33 eulerZYX(int16_t rz, int16_t ry, int16_t rx,
                                 const psyqo::Trig<>& trig) {
    auto mz = psyqo::SoftMath::generateRotationMatrix33(
        -ootAngle(rz), psyqo::SoftMath::Axis::Z, trig);
    auto my = psyqo::SoftMath::generateRotationMatrix33(
        -ootAngle(ry), psyqo::SoftMath::Axis::Y, trig);
    auto mx = psyqo::SoftMath::generateRotationMatrix33(
        -ootAngle(rx), psyqo::SoftMath::Axis::X, trig);
    psyqo::Matrix33 zy, zyx;
    psyqo::SoftMath::multiplyMatrix33(my, mz, &zy);
    psyqo::SoftMath::multiplyMatrix33(mx, zy, &zyx);
    return zyx;
}

// ── OoT math helpers ────────────────────────────────────────────────────

// s16 smooth step (from OoT Math_SmoothStepToS):
// Proportional approach with min/max step clamping.
static int16_t smoothStepAngle(int16_t current, int16_t target,
                                int16_t scale, int16_t maxStep) {
    int16_t diff = target - current;
    if (diff == 0) return 0;
    int16_t step = diff / scale;
    if (step > maxStep)  step = maxStep;
    if (step < -maxStep) step = -maxStep;
    if (step == 0) step = (diff > 0) ? 1 : -1;
    return step;
}

// sin/cos for OoT s16 binary angles using psyqo trig
// Returns 4.12 fixed-point result.
static int32_t ootSin(int16_t angle) {
    auto a = ootAngle(angle);
    return app.m_trig.sin(a).raw();
}
static int32_t ootCos(int16_t angle) {
    auto a = ootAngle(angle);
    return app.m_trig.cos(a).raw();
}

// ── Player movement (OoT-style analog stick) ────────────────────────────

// Animation indices in SKM: 0=idle, 1=walk, 2=run
constexpr int ANIM_IDLE = 0;
constexpr int ANIM_WALK = 1;
constexpr int ANIM_RUN  = 2;

// Movement constants (OoT scale - 1 unit = 1cm in model space)
constexpr int32_t SPEED_WALK     = 3 * 4096;   // 3.0 in 4.12 fixed
constexpr int32_t SPEED_RUN      = 6 * 4096;   // 6.0 in 4.12 fixed
constexpr int32_t STICK_DEADZONE = 20;          // out of 128
constexpr int32_t DECEL_RATE     = 2048;        // 0.5 per frame in 4.12
constexpr int16_t TURN_SCALE     = 4;           // Math_SmoothStepToS scale
constexpr int16_t TURN_MAX_STEP  = 3000;        // ~16° per frame max turn

// Integer atan2 approximation for stick input → OoT s16 binary angle.
// Returns angle where 0=+Z (forward), 0x4000=+X, 0x8000=-Z, 0xC000=-X.
// Uses octant lookup with linear interpolation - no floating point.
static int16_t iatan2(int32_t x, int32_t y) {
    if (x == 0 && y == 0) return 0;
    // Determine octant
    int32_t ax = x < 0 ? -x : x;
    int32_t ay = y < 0 ? -y : y;
    // atan(min/max) mapped to 0..0x2000 (45°) linearly
    int32_t mn = (ax < ay) ? ax : ay;
    int32_t mx = (ax < ay) ? ay : ax;
    int16_t a = static_cast<int16_t>((mn * 0x2000) / mx);
    // Map to full circle based on octant
    if (ax > ay) a = 0x4000 - a;       // past 45° from +Z axis
    if (y < 0)   a = static_cast<int16_t>(0x8000 - a);
    if (x < 0)   a = -a;
    return a;
}

void RoomScene::applyWallCollision() {
    if (!m_colData) return;
    constexpr int32_t WALL_STEP_UP = 50;  // match floor STEP_UP
    auto wall = Collision::findWalls(m_colData, m_skelX, m_skelY, m_skelZ,
                                      Collision::PLAYER_HEIGHT, Collision::WALL_RADIUS,
                                      WALL_STEP_UP);
    if (wall.hit) {
        m_skelX += wall.pushX;
        m_skelZ += wall.pushZ;
    }
}

void RoomScene::applyFloorCollision() {
    if (!m_colData) return;

    constexpr int32_t STEP_UP  = 50;     // allow stepping up 50 OoT units
    constexpr int32_t GRAVITY  = 1024;   // 0.25/frame in 4.12 fixed-point
    constexpr int32_t TERMINAL = -8192;  // -2.0/frame in 4.12

    auto floor = Collision::findFloor(m_colData, m_skelX, m_skelZ, m_skelY + STEP_UP);

    if (floor.floorY != INT32_MIN) {
        if (m_skelY <= floor.floorY) {
            // On or below the floor: snap up to surface
            m_skelY = floor.floorY;
            m_velY = 0;
            m_onGround = true;
        } else {
            // Above the floor: apply gravity
            m_velY -= GRAVITY;
            if (m_velY < TERMINAL) m_velY = TERMINAL;
            int32_t newY = m_skelY + (m_velY >> 12);
            if (newY <= floor.floorY) {
                m_skelY = floor.floorY;
                m_velY = 0;
                m_onGround = true;
            } else {
                m_skelY = newY;
                m_onGround = false;
            }
        }
    } else {
        // No floor found: fall
        m_velY -= GRAVITY;
        if (m_velY < TERMINAL) m_velY = TERMINAL;
        m_skelY += m_velY >> 12;
        m_onGround = false;
    }
}

void RoomScene::applyCeilingCollision() {
    if (!m_colData) return;

    auto ceil = Collision::findCeiling(m_colData, m_skelX, m_skelZ,
                                        m_skelY, Collision::CEILING_CHECK);
    if (ceil.ceilingY != INT32_MAX) {
        // Ceiling found within player height - clamp position and kill upward velocity
        int32_t maxY = ceil.ceilingY - Collision::CEILING_CHECK;
        if (m_skelY > maxY) {
            m_skelY = maxY;
            if (m_velY > 0) m_velY = 0;
        }
    }
}

void RoomScene::checkWater() {
    if (!m_colData) return;

    auto water = Collision::checkWaterBoxes(m_colData, m_skelX, m_skelY, m_skelZ);
    m_waterSurfaceY = water.surfaceY;
    m_inWater = water.inWater;
}

// ── Room transition detection (ported from OoT z_play.c) ─────────────────
//
// OoT uses plane-crossing detection, not proximity triggers.
// Each transition actor defines a plane at the doorway via rot_y.
// The dot product of (player - transition_pos) with (sin(rot_y), cos(rot_y))
// determines which side of the plane the player is on:
//   dot >= 0 → front side (front_room)
//   dot <  0 → back side  (back_room)
// When the current room doesn't match the expected room for the player's
// side, a room transition fires. This naturally prevents re-triggering
// because after the room swap the player is on the correct side.

constexpr int32_t TRANSITION_BOX_XZ   = 120;  // OoT uses ~100; slightly wider for PS1 wall collision
constexpr int32_t TRANSITION_BOX_Y    = 200;  // vertical half-extent (OoT uses 200)
constexpr int32_t TRANSITION_PRELOAD  = 500;  // XZ distance to start preloading
constexpr int     TRANSITION_COOLDOWN = 15;   // frames of immunity after transition

void RoomScene::checkTransitions() {
    if (!m_trnData || m_loading) return;
    if (m_transitionCooldown > 0) { m_transitionCooldown--; return; }

    const auto* hdr = TRN::header(m_trnData);
    const auto* ents = TRN::entries(m_trnData);

    for (int i = 0; i < hdr->num_transitions; i++) {
        const auto& e = ents[i];

        // Only consider transitions that connect to our current room
        bool connFront = (e.front_room == m_localRoomIdx);
        bool connBack  = (e.back_room  == m_localRoomIdx);
        if (!connFront && !connBack) continue;

        int targetRoom = connFront ? e.back_room : e.front_room;
        if (targetRoom < 0 || targetRoom >= SCENES[m_sceneIdx].num_rooms) continue;

        // Player offset from transition point
        int32_t dx = m_skelX - e.pos_x;
        int32_t dy = m_skelY - e.pos_y;
        int32_t dz = m_skelZ - e.pos_z;
        int32_t adx = dx < 0 ? -dx : dx;
        int32_t ady = dy < 0 ? -dy : dy;
        int32_t adz = dz < 0 ? -dz : dz;

        // Preload at wider range
        if (adx < TRANSITION_PRELOAD && adz < TRANSITION_PRELOAD && ady < TRANSITION_BOX_Y) {
            preloadRoom(targetRoom);
        }

        // Box proximity check (OoT: fabsf(dx) < 100, fabsf(dz) < 100, fabsf(dy) < 200)
        if (adx > TRANSITION_BOX_XZ || adz > TRANSITION_BOX_XZ || ady > TRANSITION_BOX_Y)
            continue;

        // Plane-crossing check: dot product with transition facing direction
        // rot_y normal points from front→back. dot >= 0 → front side.
        int32_t sinR = ootSin(e.rot_y);
        int32_t cosR = ootCos(e.rot_y);
        int32_t dot = (dx * sinR + dz * cosR) >> 12;

        int expectedRoom = (dot >= 0) ? e.back_room : e.front_room;

        // Player is on the correct side for their current room - no transition
        if (expectedRoom == m_localRoomIdx) continue;

        // Player crossed the plane → swap room (keep player position + velocity)
        m_transitionCooldown = TRANSITION_COOLDOWN;
        m_localRoomIdx = expectedRoom;

        if (m_preloadReady && m_preloadRoom == expectedRoom) {
            // Fast path: preloaded data ready - swap buffer slot
            m_activeSlot = 1 - m_activeSlot;
            m_prm = m_roomBuf[m_activeSlot].data();
            m_needUpload = true;
            m_preloadReady = false;
            m_preloadRoom = -1;
        } else {
            // Slow path: blocking load
            m_prm = nullptr;
            m_preloadReady = false;
            m_preloadRoom = -1;
            m_preloading = false;
            m_loading = true;

            const auto& scene = SCENES[m_sceneIdx];
            int globalIdx = scene.room_base + expectedRoom;
            app.m_loader.readFile(ROOM_FILES[globalIdx], app.m_isoParser,
                [this](psyqo::Buffer<uint8_t>&& buffer) {
                    m_roomBuf[m_activeSlot] = eastl::move(buffer);
                    m_prm = m_roomBuf[m_activeSlot].data();
                    m_needUpload = (m_prm != nullptr);
                    m_loading = false;
                });
        }
        return;
    }
}

void RoomScene::updatePlayer() {
    using APad = psyqo::AdvancedPad;
    constexpr auto P1 = APad::Pad::Pad1a;

    // Read left analog stick (0x00-0xFF, center 0x80)
    int32_t rawX = app.m_pad.getAdc(P1, 2) - 0x80;  // -128..+127
    int32_t rawY = app.m_pad.getAdc(P1, 3) - 0x80;

    // Compute stick magnitude (Manhattan approximation: max + min/2)
    int32_t absX = rawX < 0 ? -rawX : rawX;
    int32_t absY = rawY < 0 ? -rawY : rawY;
    int32_t mag = (absX > absY) ? absX + (absY >> 1) : absY + (absX >> 1);

    bool moving = mag > STICK_DEADZONE;

    if (moving) {
        // Camera yaw in OoT binary angle space.
        // psyqo::Angle is FixedPoint<10>, full circle = 2048.
        // OoT binary angle full circle = 65536. Ratio = 32.
        int16_t camYaw = static_cast<int16_t>(m_camRotY.raw() * 32);

        // Stick angle: iatan2(-x, -y) - forward is stick-up (negative Y)
        // X negated to match un-mirrored rendering (screen +X = world +X)
        int16_t inputYaw = iatan2(-rawX, -rawY);

        // Target yaw = camera yaw + stick angle (OoT formula)
        int16_t targetYaw = camYaw + inputYaw;

        // Smooth turn toward target
        int16_t step = smoothStepAngle(m_playerRotY, targetYaw,
                                        TURN_SCALE, TURN_MAX_STEP);
        m_playerRotY += step;

        // Speed: walk normally, run while holding Circle (Souls-like)
        bool running = app.m_pad.isButtonPressed(P1, APad::Circle);
        int32_t targetSpeed = running ? SPEED_RUN : SPEED_WALK;
        // Smooth acceleration
        if (m_playerSpeed < targetSpeed)
            m_playerSpeed += 1024;  // ~0.25 per frame
        if (m_playerSpeed > targetSpeed)
            m_playerSpeed = targetSpeed;
    } else {
        // Decelerate to zero
        if (m_playerSpeed > 0) {
            m_playerSpeed -= DECEL_RATE;
            if (m_playerSpeed < 0) m_playerSpeed = 0;
        }
    }

    // Move in facing direction (OoT Actor_MoveWithGravity)
    if (m_playerSpeed > 0) {
        // velocity = speed * sin/cos(facingYaw), in 4.12 × 4.12 = shift 12
        int32_t vx = (m_playerSpeed * ootSin(m_playerRotY)) >> 12;
        int32_t vz = (m_playerSpeed * ootCos(m_playerRotY)) >> 12;
        // Speed is in OoT units (model space). World pos is also OoT units.
        // Shift by 12 to get integer displacement per frame.
        m_skelX += vx >> 12;
        m_skelZ += vz >> 12;
    }

    // Collision: wall push → floor snap → ceiling clamp → water detect
    applyWallCollision();
    applyFloorCollision();
    applyCeilingCollision();
    checkWater();

    // Room transition detection (after collision, before animation)
    checkTransitions();

    // Animation state: select anim based on speed
    int newAnim;
    if (m_playerSpeed == 0)
        newAnim = ANIM_IDLE;
    else if (m_playerSpeed < SPEED_RUN)
        newAnim = ANIM_WALK;
    else
        newAnim = ANIM_RUN;

    if (newAnim != m_animIdx) {
        m_animIdx = newAnim;
        m_animFrame = 0;
    }
}

// ── Skeleton loading via CD-ROM ──────────────────────────────────────────

void RoomScene::loadSkeleton() {
    app.m_loader.readFile("LINK.SKM;1", app.m_isoParser,
        [this](psyqo::Buffer<uint8_t>&& buffer) {
            m_skelBuf = eastl::move(buffer);
            if (m_skelBuf.size() > sizeof(SKM::Header)) {
                m_skm = m_skelBuf.data();
                m_limbCache.build(m_skm);
                m_skelLoaded = true;
            }
            loadScene(3, 0);
        });
}

// ── Bone hierarchy computation ───────────────────────────────────────────

void RoomScene::computeBones(const int16_t* frame) {
    int16_t rootX, rootY, rootZ;
    SKM::frameRootPos(frame, rootX, rootY, rootZ);

    // Root bone: anim rotation × player facing rotation
    int16_t rz, ry, rx;
    SKM::frameLimbRot(frame, 0, rz, ry, rx);
    auto animRot = eulerZYX(rz, ry, rx, app.m_trig);

    // Apply player facing direction as Y rotation on root
    auto facingRot = psyqo::SoftMath::generateRotationMatrix33(
        -ootAngle(m_playerRotY), psyqo::SoftMath::Axis::Y, app.m_trig);
    psyqo::SoftMath::multiplyMatrix33(animRot, facingRot, &m_bones[0].rot);

    m_bones[0].tx = rootX;
    m_bones[0].ty = rootY;
    m_bones[0].tz = rootZ;

    const auto* ls = SKM::limbs(m_skm);
    if (ls[0].child != 0xFF)
        computeBoneRecurse(ls[0].child, m_bones[0], frame);
}

void RoomScene::computeBoneRecurse(int limbIdx, const BoneState& parent,
                                    const int16_t* frame) {
    const auto* ls = SKM::limbs(m_skm);
    const auto& limb = ls[limbIdx];

    // World rotation = parent × limb local rotation from animation frame
    int16_t rz, ry, rx;
    SKM::frameLimbRot(frame, limbIdx, rz, ry, rx);
    auto limbRot = eulerZYX(rz, ry, rx, app.m_trig);
    psyqo::SoftMath::multiplyMatrix33(limbRot, parent.rot, &m_bones[limbIdx].rot);

    int32_t jx = limb.joint_x, jy = limb.joint_y, jz = limb.joint_z;
    auto& pr = parent.rot;
    m_bones[limbIdx].tx = parent.tx +
        ((pr.vs[0].x.raw() * jx + pr.vs[0].y.raw() * jy + pr.vs[0].z.raw() * jz) >> 12);
    m_bones[limbIdx].ty = parent.ty +
        ((pr.vs[1].x.raw() * jx + pr.vs[1].y.raw() * jy + pr.vs[1].z.raw() * jz) >> 12);
    m_bones[limbIdx].tz = parent.tz +
        ((pr.vs[2].x.raw() * jx + pr.vs[2].y.raw() * jy + pr.vs[2].z.raw() * jz) >> 12);

    if (limb.child != 0xFF)
        computeBoneRecurse(limb.child, m_bones[limbIdx], frame);
    if (limb.sibling != 0xFF)
        computeBoneRecurse(limb.sibling, parent, frame);
}

// ── Skeleton rendering ───────────────────────────────────────────────────

void RoomScene::renderSkeleton(const psyqo::Matrix33& renderRot,
                                int32_t camTX, int32_t camTY, int32_t camTZ) {
    if (!m_skm) return;

    const auto* shdr = SKM::header(m_skm);

    // Advance animation
    m_animFrame++;
    const auto* ad = &SKM::animDescs(m_skm)[m_animIdx];
    if (m_animFrame >= ad->frame_count) {
        m_animFrame = (ad->flags & 1) ? 0 : ad->frame_count - 1;
    }

    const int16_t* frame = SKM::animFrame(m_skm, m_animIdx, m_animFrame);
    computeBones(frame);

    for (int i = 0; i < shdr->num_limbs; i++) {
        drawLimb(i, renderRot, camTX, camTY, camTZ);
    }
}

void RoomScene::drawLimb(int limbIdx, const psyqo::Matrix33& renderRot,
                          int32_t camTX, int32_t camTY, int32_t camTZ) {
    const auto* ls = SKM::limbs(m_skm);
    if (ls[limbIdx].num_verts == 0 || ls[limbIdx].num_tris == 0) return;

    // View-space rotation = camera × bone world rotation
    psyqo::Matrix33 viewRot;
    psyqo::SoftMath::multiplyMatrix33(m_bones[limbIdx].rot, renderRot, &viewRot);

    // Bone positions are in model space (100x world). Convert skeleton world
    // pos + camera translation to model space so GTE works in a single space.
    int32_t bx = m_bones[limbIdx].tx + m_skelX * SKEL_SCALE;
    int32_t by = m_bones[limbIdx].ty + m_skelY * SKEL_SCALE;
    int32_t bz = m_bones[limbIdx].tz + m_skelZ * SKEL_SCALE;
    int32_t vtx = ((renderRot.vs[0].x.raw() * bx +
                    renderRot.vs[0].y.raw() * by +
                    renderRot.vs[0].z.raw() * bz) >> 12) + camTX * SKEL_SCALE;
    int32_t vty = ((renderRot.vs[1].x.raw() * bx +
                    renderRot.vs[1].y.raw() * by +
                    renderRot.vs[1].z.raw() * bz) >> 12) + camTY * SKEL_SCALE;
    int32_t vtz = ((renderRot.vs[2].x.raw() * bx +
                    renderRot.vs[2].y.raw() * by +
                    renderRot.vs[2].z.raw() * bz) >> 12) + camTZ * SKEL_SCALE;

    // Write per-limb view matrix to GTE
    psyqo::GTE::writeUnsafe<psyqo::GTE::PseudoRegister::Rotation>(viewRot);
    psyqo::GTE::write<psyqo::GTE::Register::TRX, psyqo::GTE::Unsafe>(
        static_cast<uint32_t>(vtx));
    psyqo::GTE::write<psyqo::GTE::Register::TRY, psyqo::GTE::Unsafe>(
        static_cast<uint32_t>(vty));
    psyqo::GTE::write<psyqo::GTE::Register::TRZ, psyqo::GTE::Unsafe>(
        static_cast<uint32_t>(vtz));

    // Transform limb vertices
    const auto* pos = m_limbCache.positions(m_skm, limbIdx);
    transformVertices(reinterpret_cast<const PRM::Pos*>(pos), ls[limbIdx].num_verts);

    // Emit textured triangles
    const auto* uv = m_limbCache.uvs(m_skm, limbIdx);
    const auto* tri = m_limbCache.triangles(m_skm, limbIdx);
    const auto* shdr = SKM::header(m_skm);

    for (int t = 0; t < ls[limbIdx].num_tris && m_triCount < MAX_TRIS; t++) {
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
        maxZ /= SKEL_SCALE;
        int32_t otIdx = static_cast<int32_t>((maxZ * OT_SIZE) >> 12);
        if (otIdx <= 0 || otIdx >= OT_SIZE) continue;

        auto& frag = m_tris[m_parity][m_triCount];
        auto& p = frag.primitive;

        p.pointA.x = sv0.sx; p.pointA.y = sv0.sy;
        p.pointB.x = sv1.sx; p.pointB.y = sv1.sy;
        p.pointC.x = sv2.sx; p.pointC.y = sv2.sy;

        // OoT character vertices store normals (not colors) in bytes 12-15.
        // Use neutral gray (128 = 1.0× texture modulation) until GTE lighting is added.
        psyqo::Color neutral{{.r = 128, .g = 128, .b = 128}};
        p.setColorA(neutral);
        p.setColorB(neutral);
        p.setColorC(neutral);

        int texSlot = m_skelTexBase + idx.tex_id;
        if (idx.tex_id >= shdr->num_textures || texSlot >= g_vramAlloc.numSlots())
            continue;

        const auto& ti = g_vramAlloc.info(texSlot);
        p.uvA.u = uv[idx.v0].u + ti.u_off;
        p.uvA.v = uv[idx.v0].v + ti.v_off;
        p.uvB.u = uv[idx.v1].u + ti.u_off;
        p.uvB.v = uv[idx.v1].v + ti.v_off;
        p.uvC.u = uv[idx.v2].u + ti.u_off;
        p.uvC.v = uv[idx.v2].v + ti.v_off;
        p.tpage = ti.tpage;
        p.clutIndex = ti.clut;

        m_ots[m_parity].insert(frag, otIdx);
        auto& tw = m_texWins[m_parity][m_triCount];
        tw.primitive.command = ti.tex_window;
        m_ots[m_parity].insert(tw, otIdx);
        m_triCount++;
    }
}
