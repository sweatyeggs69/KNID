#include "app/licenses.h"

#include <cstdio>

extern const unsigned char knid_text_license[];
extern const unsigned knid_text_license_size;
extern const unsigned char knid_text_notices[];
extern const unsigned knid_text_notices_size;
extern const unsigned char knid_text_fredoka_ofl[];
extern const unsigned knid_text_fredoka_ofl_size;

namespace knid::licenses {

namespace {
std::string_view view(const unsigned char* p, unsigned n) { return {reinterpret_cast<const char*>(p), n}; }
}  // namespace

std::string_view knid() { return view(knid_text_license, knid_text_license_size); }
std::string_view notices() { return view(knid_text_notices, knid_text_notices_size); }
std::string_view fredoka_ofl() { return view(knid_text_fredoka_ofl, knid_text_fredoka_ofl_size); }

void print_all() {
    for (std::string_view t : {knid(), notices(), fredoka_ofl()}) {
        std::fwrite(t.data(), 1, t.size(), stdout);
        std::fputs("\n\n", stdout);
    }
}

}  // namespace knid::licenses
