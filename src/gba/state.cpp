// Save state serialization. Members are written in a fixed order as raw bytes; a fingerprint of
// the struct sizes guards against loading a state from a build with a different layout.
#include "gba.h"

#include <cstring>
#include <type_traits>

namespace knid::gba {

namespace {
constexpr char kMagic[8] = {'K', 'N', 'I', 'D', 'G', 'B', 'A', 'S'};
constexpr u32 kVersion = 1;

class Writer {
public:
    explicit Writer(std::vector<u8>& out) : out_(out) {}
    template <class T>
    void put(const T& v) {
        static_assert(std::is_trivially_copyable_v<T>);
        const u8* p = reinterpret_cast<const u8*>(&v);
        out_.insert(out_.end(), p, p + sizeof(T));
    }
private:
    std::vector<u8>& out_;
};

class Reader {
public:
    explicit Reader(const std::vector<u8>& in) : in_(in) {}
    template <class T>
    bool get(T& v) {
        static_assert(std::is_trivially_copyable_v<T>);
        if (pos_ + sizeof(T) > in_.size()) return false;
        std::memcpy(&v, in_.data() + pos_, sizeof(T));
        pos_ += sizeof(T);
        return true;
    }
    bool done() const { return pos_ == in_.size(); }
private:
    const std::vector<u8>& in_;
    size_t pos_ = 0;
};
}  // namespace

// The order of fields here is the format. Bump kVersion when it changes.
#define KNID_STATE_FIELDS(X)                                                                    \
    X(ewram_) X(iwram_) X(io_) X(pal_) X(vram_) X(oam_) X(sram_) X(sram_dirty_)                  \
    X(r_) X(cpsr_) X(bank_r13_) X(bank_r14_) X(bank_spsr_) X(bank_fiq_r8_) X(bank_usr_r8_)        \
    X(pc_) X(halted_) X(intr_wait_) X(bios_latch_) X(last_fetch_) X(wait_n_) X(wait_s_)          \
    X(keys_pressed_) X(dma_) X(timer_)                                                          \
    X(line_cycle_) X(vcount_) X(hblank_) X(bg_ref_x_) X(bg_ref_y_) X(frame_) X(frame_count_)     \
    X(sq_) X(wave_) X(wave_ram_) X(noise_) X(fifo_) X(apu_sample_acc_) X(apu_seq_acc_)          \
    X(apu_seq_step_)

// A state saved under one BIOS mode (real BIOS vs. HLE) can be loaded under the other only at a
// point where no BIOS code is mid-way: the game itself running, or the game waiting in
// IntrWait/VBlankIntrWait, which is then simply executed again under the new mode.
bool Gba::convert_bios_mode(Gba& t, bool from_real) {
    const u32 mode = t.cpsr_ & 0x1F;
    if (mode == 0x12) return false;  // inside an IRQ (the dispatcher differs between modes)
    if (!from_real) {
        // HLE -> real BIOS. Only our IRQ stub lives in the BIOS region under HLE.
        if (t.pc_ < 0x4000) return false;
        if (t.intr_wait_) {  // re-execute the SWI at pc_ through the real BIOS
            t.intr_wait_ = false;
            t.halted_ = false;
        }
        return true;
    }
    // Real BIOS -> HLE.
    if (t.pc_ >= 0x4000) return true;  // game code (not inside a SWI)
    if (mode != 0x13) return false;  // only a SWI (SVC mode) can be unwound
    const u32 caller = t.bank_spsr_[3];
    if ((caller & 0x1F) == 0x12 || (caller & 0x1F) == 0x13) return false;
    const bool thumb = caller & 0x20;
    const u32 swi_at = t.r_[14] - (thumb ? 2 : 4);
    auto rd8 = [&](u32 a) -> int {
        switch (a >> 24) {
            case 0x02: return t.ewram_[a & 0x3FFFF];
            case 0x03: return t.iwram_[a & 0x7FFF];
            case 0x08: case 0x09: case 0x0A: case 0x0B: case 0x0C: case 0x0D:
                return (a & 0x01FFFFFF) < rom_.size() ? rom_[a & 0x01FFFFFF] : -1;
            default: return -1;
        }
    };
    int b0 = rd8(swi_at), b1 = rd8(swi_at + 1), b2 = rd8(swi_at + 2), b3 = thumb ? 0 : rd8(swi_at + 3);
    if (b0 < 0 || b1 < 0 || b2 < 0 || b3 < 0) return false;
    u32 number;
    if (thumb) {
        if (b1 != 0xDF) return false;
        number = u32(b0);
    } else {
        if ((b3 & 0x0F) != 0x0F) return false;
        number = u32(b2);
    }
    if (number != 4 && number != 5) return false;  // only IntrWait / VBlankIntrWait
    // Leave the BIOS: back to the caller's mode with the SVC stack reset, then run the SWI again.
    t.r_[13] = 0x03007FE0;
    t.set_cpsr(caller);
    t.pc_ = swi_at;
    t.halted_ = false;
    t.intr_wait_ = false;
    return true;
}

std::vector<u8> Gba::save_state() const {
    std::vector<u8> out;
    out.reserve(0x60000);
    Writer w(out);
    for (char c : kMagic) w.put(c);
    w.put(kVersion);
    u32 fingerprint = 0;
#define KNID_FP(m) fingerprint = fingerprint * 131 + u32(sizeof(m));
    KNID_STATE_FIELDS(KNID_FP)
#undef KNID_FP
    w.put(fingerprint);
    w.put(real_bios_);
#define KNID_PUT(m) w.put(m);
    KNID_STATE_FIELDS(KNID_PUT)
#undef KNID_PUT
    return out;
}

bool Gba::load_state(const std::vector<u8>& data, std::string* error) {
    auto fail = [&](const char* why) {
        if (error) *error = why;
        return false;
    };
    Reader r(data);
    char magic[8];
    for (char& c : magic)
        if (!r.get(c)) return fail("The save state is truncated.");
    if (std::memcmp(magic, kMagic, sizeof magic) != 0) return fail("This is not a KNID save state.");
    u32 version = 0, fingerprint = 0, expected = 0;
    bool state_real_bios = false;
    if (!r.get(version) || version != kVersion)
        return fail("This save state was made by a different version of KNID and can't be loaded.");
#define KNID_FP(m) expected = expected * 131 + u32(sizeof(m));
    KNID_STATE_FIELDS(KNID_FP)
#undef KNID_FP
    if (!r.get(fingerprint) || fingerprint != expected)
        return fail("This save state was made by a different build of KNID and can't be loaded.");
    if (!r.get(state_real_bios)) return fail("The save state is truncated.");

    // Decode into a copy first so a bad file never leaves the machine half-loaded.
    auto tmp = std::make_unique<Gba>(std::vector<u8>{}, std::filesystem::path{});
    Gba& t = *tmp;
#define KNID_GET(m) if (!r.get(t.m)) return fail("The save state is truncated.");
    KNID_STATE_FIELDS(KNID_GET)
#undef KNID_GET
    if (!r.done()) return fail("The save state has unexpected extra data.");
    if (state_real_bios != real_bios_ && !convert_bios_mode(t, state_real_bios))
        return fail("This save state was made with a different BIOS setting at a moment that can't be "
                    "converted (the BIOS or an interrupt was running). Switch the BIOS setting back to load it.");

#define KNID_COPY(m) m = t.m;
    KNID_STATE_FIELDS(KNID_COPY)
#undef KNID_COPY
    branched_ = false;
    cycles_ = 0;
    frame_done_ = false;
    audio_.clear();
    sram_dirty_ = false;  // the save file is updated the next time the game writes SRAM
    sram_dirty_frames_ = 0;
    wide_level_ = {};
    return true;
}

#undef KNID_STATE_FIELDS

}  // namespace knid::gba
