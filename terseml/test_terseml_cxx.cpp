// The header must be usable from C++ -- that is what extern "C" is for, and
// it is only true if someone compiles it that way.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "terseml.h"

int main() {
    tsml_doc *a = nullptr;
    if (tsml_builder_create(&a) != TSML_OK) return 1;
    tsml_node *el = tsml_element(a, reinterpret_cast<const uint8_t *>("p"), 1);
    tsml_add_child(a, el, tsml_text(a, reinterpret_cast<const uint8_t *>("hi"), 2));
    uint8_t *out = nullptr;
    size_t len = 0;
    if (tsml_encode(el, &out, &len) != TSML_OK) return 1;
    bool ok = len == 6 && std::memcmp(out, "{p,hi}", 6) == 0;
    std::printf("C++ build: %s (%.*s)\n", ok ? "ok" : "FAIL", static_cast<int>(len),
                reinterpret_cast<char *>(out));
    std::free(out);
    tsml_doc_free(&a);
    return ok ? 0 : 1;
}
