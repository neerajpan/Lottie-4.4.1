// Android NDK r26+ compat stub
// __libcpp_verbose_abort was added in NDK r26 as a weak external symbol.
// With static libc++ (godot-cpp 4.4.1 + NDK r27+), the symbol may remain
// unresolved at runtime. This stub satisfies the reference at link time.

#if defined(__ANDROID__)
#include <cstdlib>
#include <cstdarg>

extern "C" __attribute__((weak))
void __libcpp_verbose_abort(const char* /*format*/, ...) {
    abort();
}
#endif
