// VRAM Allocator - manages PS1 VRAM layout for textures and CLUTs.
//
// PS1 VRAM is 1024×512 @ 16bpp. Layout:
//   X=0..319,  Y=0..479:  framebuffers (2× 320×240)
//   X=0..319,  Y=480..495: FastFill danger zone (cleared every frame)
//   X=320..1023, Y=0..495: texture pixel data (strip-packed, aligned)
//   X=0..1023, Y=496..511: CLUT data (16 rows)
//
// Textures are aligned to their own dimensions within each TPage so that
// the PS1 texture window (GP0 E2h) can handle per-pixel UV wrapping.
// This matches the N64 RDP's automatic texture tiling behavior.

#pragma once

#include <stdint.h>
#include "psyqo/primitives/common.hh"

namespace VramAlloc {

struct TexInfo {
    psyqo::PrimPieces::TPageAttr tpage;
    psyqo::PrimPieces::ClutIndex clut;
    uint8_t u_off, v_off;
    uint8_t u_mask, v_mask;  // texel_w-1, texel_h-1
    uint32_t tex_window;     // pre-computed GP0(E2h) command word
};

static constexpr int MAX_TEXTURES = 96;

// Texture region: right of framebuffers, above CLUT rows
static constexpr int16_t TEX_X0   = 320;
static constexpr int16_t TEX_X1   = 1024;
static constexpr int16_t TEX_Y0   = 0;
static constexpr int16_t TEX_Y1   = 496;

// CLUT region: bottom 16 rows, full width
static constexpr int16_t CLUT_X1  = 1024;
static constexpr int16_t CLUT_Y0  = 496;
static constexpr int16_t CLUT_Y1  = 512;

struct Slot {
    TexInfo info;
    int16_t vram_x, vram_y;    // pixel data position in VRAM
    int16_t vram_w, vram_h;    // pixel data size (VRAM pixels)
    int16_t clut_x, clut_y;    // CLUT position in VRAM
    int16_t clut_w;            // CLUT width (VRAM pixels, 16-aligned)
};

class Allocator {
  public:
    void reset() {
        m_numSlots = 0;
        m_texCurX  = TEX_X0;
        m_texCurY  = TEX_Y0;
        m_texRowH  = 0;
        m_clutCurX = 0;
        m_clutCurY = CLUT_Y0;
    }

    // Allocate VRAM for one texture + its CLUT. Returns slot index, or -1.
    int alloc(uint16_t texel_w, uint16_t texel_h,
              uint8_t format,           // 0=4bit, 1=8bit
              uint16_t num_clut_colors) {
        if (m_numSlots >= MAX_TEXTURES) return -1;

        // Compute VRAM pixel width for this texture
        int16_t vw = (format == 0) ? (texel_w / 4) : (texel_w / 2);
        int16_t vh = static_cast<int16_t>(texel_h);

        // Texels per VRAM pixel (for alignment calculations)
        int16_t tpv = (format == 0) ? 4 : 2;

        // TPage-aware strip packing.
        // PS1 textures must not straddle a texture page boundary:
        //   4-bit pages span 64 VRAM pixels (256 texels)
        //   8-bit pages span 128 VRAM pixels (256 texels)
        // TPage base is always at a multiple of 64 VRAM pixels.
        int16_t page_span = (format == 0) ? 64 : 128;

        // Texture window alignment: texture must start at a multiple of its
        // own dimensions within the TPage, so GP0(E2h) offsets work correctly.
        // Alignment in VRAM pixels = texel_w / texels_per_vram_pixel = vw.
        // Also align Y to texel_h within each 256-row TPage block.
        int16_t align_x = (texel_w >= 8) ? vw : 1;
        int16_t align_y = (texel_h >= 8) ? vh : 1;

        auto alignUp = [](int16_t v, int16_t a) -> int16_t {
            return static_cast<int16_t>(((v + a - 1) / a) * a);
        };

        auto fitsAt = [&](int16_t x, int16_t y) {
            return (x % 64) + vw <= page_span && x + vw <= TEX_X1 &&
                   y + vh <= TEX_Y1;
        };

        // Align X cursor within current row
        int16_t ax = alignUp(m_texCurX, align_x);
        int16_t ay = alignUp(m_texCurY, align_y);

        if (!fitsAt(ax, ay)) {
            // Try next 64-px page boundary on same row (aligned)
            int16_t next = static_cast<int16_t>(((m_texCurX + 63) / 64) * 64);
            int16_t anext = alignUp(next, align_x);
            if (fitsAt(anext, ay)) {
                ax = anext;
            } else {
                // Wrap to next row (aligned)
                ay = alignUp(ay + m_texRowH, align_y);
                ax = alignUp(TEX_X0, align_x);
                m_texRowH = 0;
                if (!fitsAt(ax, ay)) return -1;  // out of space
            }
        }

        int16_t vx = ax;
        int16_t vy = ay;
        m_texCurX = vx + vw;
        m_texCurY = vy;
        if (vh > m_texRowH) m_texRowH = vh;

        // CLUT: 16-pixel aligned width
        int16_t cw = static_cast<int16_t>((num_clut_colors + 15) & ~15);
        if (m_clutCurX + cw > CLUT_X1) {
            m_clutCurY++;
            m_clutCurX = 0;
        }
        if (m_clutCurY >= CLUT_Y1) return -1;  // out of CLUT space

        int16_t cx = m_clutCurX;
        int16_t cy = m_clutCurY;
        m_clutCurX += cw;

        // Compute TPage and offsets
        auto& s = m_slots[m_numSlots];
        s.vram_x = vx;
        s.vram_y = vy;
        s.vram_w = vw;
        s.vram_h = vh;
        s.clut_x = cx;
        s.clut_y = cy;
        s.clut_w = cw;

        s.info.tpage = psyqo::PrimPieces::TPageAttr();
        s.info.tpage.setPageX(vx / 64).setPageY(vy / 256);
        if (format == 0)
            s.info.tpage.set(psyqo::Prim::TPageAttr::Tex4Bits);
        else
            s.info.tpage.set(psyqo::Prim::TPageAttr::Tex8Bits);

        if (format == 0)
            s.info.u_off = static_cast<uint8_t>((vx % 64) * 4);
        else
            s.info.u_off = static_cast<uint8_t>((vx % 64) * 2);
        s.info.v_off = static_cast<uint8_t>(vy % 256);

        s.info.clut = psyqo::PrimPieces::ClutIndex(cx / 16, cy);

        // UV masks for wrapping (textures are power-of-2)
        s.info.u_mask = static_cast<uint8_t>(texel_w - 1);
        s.info.v_mask = static_cast<uint8_t>(texel_h - 1);

        // Pre-compute GP0(E2h) texture window command.
        // Formula: texcoord = (texcoord & ~(M*8)) | ((O & M)*8)
        // For wrapping at W texels starting at offset U in TPage:
        //   mask = (256 - W) / 8,  offset = U / 8
        // Requires W >= 8 and U aligned to W.
        if (texel_w >= 8 && texel_h >= 8) {
            uint32_t mx = (256 - texel_w) / 8;
            uint32_t my = (256 - texel_h) / 8;
            uint32_t ox = s.info.u_off / 8;
            uint32_t oy = s.info.v_off / 8;
            s.info.tex_window = 0xe2000000u
                | (mx & 0x1fu)
                | ((my & 0x1fu) << 5)
                | ((ox & 0x1fu) << 10)
                | ((oy & 0x1fu) << 15);
        } else {
            // Tiny textures: no window (mask=0 → identity)
            s.info.tex_window = 0xe2000000u;
        }

        return m_numSlots++;
    }

    const TexInfo& info(int slot) const { return m_slots[slot].info; }

    psyqo::Rect pixelRect(int slot) const {
        const auto& s = m_slots[slot];
        psyqo::Rect r;
        r.pos.x = s.vram_x;
        r.pos.y = s.vram_y;
        r.size.w = s.vram_w;
        r.size.h = s.vram_h;
        return r;
    }

    psyqo::Rect clutRect(int slot) const {
        const auto& s = m_slots[slot];
        psyqo::Rect r;
        r.pos.x = s.clut_x;
        r.pos.y = s.clut_y;
        r.size.w = s.clut_w;
        r.size.h = 1;
        return r;
    }

    int numSlots() const { return m_numSlots; }

  private:
    Slot m_slots[MAX_TEXTURES];
    int  m_numSlots = 0;

    // Texture strip packer cursor
    int16_t m_texCurX = TEX_X0;
    int16_t m_texCurY = TEX_Y0;
    int16_t m_texRowH = 0;

    // CLUT linear packer cursor
    int16_t m_clutCurX = 0;
    int16_t m_clutCurY = CLUT_Y0;
};

}  // namespace VramAlloc
