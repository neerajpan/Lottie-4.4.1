// Android NDK r26+ compat stub
// __libcpp_verbose_abort was introduced in NDK r26 libc++. When statically
// linking libc++ (godot-cpp 4.4.1 + NDK r26+), this symbol must be defined
// so the Android dynamic linker can resolve it at runtime.
// Strong definition ensures it is always present in the .so.

#if defined(__ANDROID__)
#include <cstdlib>
#include <cstdarg>

extern "C" void __libcpp_verbose_abort(const char* /*format*/, ...) {
    abort();
}
#endif
