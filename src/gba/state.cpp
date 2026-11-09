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
    if (state_real_bios != real_bios_ && t.pc_ < 0x4000)
        return fail("This save state was made with a different BIOS setting while the BIOS was running.");

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
