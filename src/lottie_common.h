#ifndef LOTTIE_COMMON_H
#define LOTTIE_COMMON_H

// Shared, node-agnostic helpers for the Lottie GDExtension.
//
// These utilities (pixel-format conversion and .lottie/.zip extraction) have
// no dependency on a specific node type, so they can be reused by both the 2D
// (LottieAnimation : Node2D) and 3D (LottieAnimation3D : Sprite3D) nodes.
//
// Header-only (all functions `inline`) so no extra build wiring is required;
// SConstruct already globs every src/*.cpp, and including this header in
// multiple translation units is ODR-safe.

#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/zip_reader.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

#if defined(__SSSE3__)
    #include <tmmintrin.h>
    #define LOTTIE_SIMD_SSSE3 1
#endif
#if defined(__ARM_NEON)
    #include <arm_neon.h>
    #define LOTTIE_SIMD_NEON 1
#endif

namespace lottie {

using namespace godot;

// Convert a premultiplied-ARGB (32-bit, ThorVG ARGB8888S) buffer to RGBA
// bytes. Uses SSSE3 / NEON byte shuffles where available, scalar otherwise.
inline void convert_argb_to_rgba(const uint32_t *src, uint8_t *dst, size_t count) {
#if LOTTIE_SIMD_SSSE3
    size_t vec_count = count / 4;
    const __m128i mask = _mm_setr_epi8(
        2, 1, 0, 3,
        6, 5, 4, 7,
        10, 9, 8, 11,
        14, 13, 12, 15);
    const __m128i *srcv = reinterpret_cast<const __m128i *>(src);
    __m128i *dstv = reinterpret_cast<__m128i *>(dst);
    for (size_t i = 0; i < vec_count; ++i) {
        __m128i pixels = _mm_loadu_si128(&srcv[i]);
        __m128i shuffled = _mm_shuffle_epi8(pixels, mask);
        _mm_storeu_si128(&dstv[i], shuffled);
    }
    size_t processed = vec_count * 4;
    for (size_t i = processed; i < count; ++i) {
        uint32_t p = src[i];
        dst[i * 4 + 0] = (uint8_t)((p >> 16) & 0xFF);
        dst[i * 4 + 1] = (uint8_t)((p >> 8) & 0xFF);
        dst[i * 4 + 2] = (uint8_t)(p & 0xFF);
        dst[i * 4 + 3] = (uint8_t)((p >> 24) & 0xFF);
    }
#elif LOTTIE_SIMD_NEON
    size_t vec_count = count / 4;
    static const uint8_t tbl_data[16] = {
        2, 1, 0, 3, 6, 5, 4, 7, 10, 9, 8, 11, 14, 13, 12, 15
    };
    uint8x16_t tbl = vld1q_u8(tbl_data);
    for (size_t i = 0; i < vec_count; ++i) {
        uint8x16_t pixels = vld1q_u8(reinterpret_cast<const uint8_t *>(&src[i * 4]));
#if defined(__aarch64__) || defined(__ARM_FEATURE_QBIT)
        uint8x16_t shuffled = vqtbl1q_u8(pixels, tbl);
#else
        uint8_t tmp[16];
        vst1q_u8(tmp, pixels);
        uint8_t out[16];
        for (int k = 0; k < 16; k++) out[k] = tmp[tbl_data[k]];
        pixels = vld1q_u8(out);
        uint8x16_t shuffled = pixels;
#endif
        vst1q_u8(&dst[i * 16], shuffled);
    }
    size_t processed = vec_count * 4;
    for (size_t i = processed; i < count; ++i) {
        uint32_t p = src[i];
        dst[i * 4 + 0] = (uint8_t)((p >> 16) & 0xFF);
        dst[i * 4 + 1] = (uint8_t)((p >> 8) & 0xFF);
        dst[i * 4 + 2] = (uint8_t)(p & 0xFF);
        dst[i * 4 + 3] = (uint8_t)((p >> 24) & 0xFF);
    }
#else
    for (size_t i = 0; i < count; ++i) {
        uint32_t p = src[i];
        dst[i * 4 + 0] = (uint8_t)((p >> 16) & 0xFF);
        dst[i * 4 + 1] = (uint8_t)((p >> 8) & 0xFF);
        dst[i * 4 + 2] = (uint8_t)(p & 0xFF);
        dst[i * 4 + 3] = (uint8_t)((p >> 24) & 0xFF);
    }
#endif
}

// Bleed edge color into fully-transparent neighbour texels so GPU bilinear
// filtering does not sample black/garbage along the animation's alpha border.
inline void fix_alpha_border_rgba(uint8_t *rgba, int w, int h) {
    if (!rgba || w <= 2 || h <= 2) return;
    std::vector<uint8_t> rgb_copy((size_t)w * (size_t)h * 3);
    for (int y = 0; y < h; ++y) {
        const uint8_t *row = rgba + (size_t)y * (size_t)w * 4;
        uint8_t *dst = rgb_copy.data() + (size_t)y * (size_t)w * 3;
        for (int x = 0; x < w; ++x) {
            dst[x * 3 + 0] = row[x * 4 + 0];
            dst[x * 3 + 1] = row[x * 4 + 1];
            dst[x * 3 + 2] = row[x * 4 + 2];
        }
    }
    auto at = [&](int x, int y) -> uint8_t * { return rgba + ((size_t)y * (size_t)w + (size_t)x) * 4; };
    auto at_rgb = [&](int x, int y) -> const uint8_t * { return rgb_copy.data() + ((size_t)y * (size_t)w + (size_t)x) * 3; };
    for (int y = 1; y < h - 1; ++y) {
        for (int x = 1; x < w - 1; ++x) {
            uint8_t *px = at(x, y);
            if (px[3] != 0) continue;
            bool copied = false;
            for (int dy = -1; dy <= 1 && !copied; ++dy) {
                for (int dx = -1; dx <= 1 && !copied; ++dx) {
                    if (dx == 0 && dy == 0) continue;
                    uint8_t *n = at(x + dx, y + dy);
                    if (n[3] > 0) {
                        const uint8_t *nrgb = at_rgb(x + dx, y + dy);
                        px[0] = nrgb[0];
                        px[1] = nrgb[1];
                        px[2] = nrgb[2];
                        copied = true;
                    }
                }
            }
        }
    }
}

// Convert premultiplied RGBA to straight-alpha RGBA in place.
inline void unpremultiply_alpha_rgba(uint8_t *rgba, int w, int h) {
    if (!rgba || w <= 0 || h <= 0) return;
    const size_t pixels = (size_t)w * (size_t)h;
    uint8_t *p = rgba;
    for (size_t i = 0; i < pixels; ++i, p += 4) {
        uint8_t a = p[3];
        if (a == 0) {
            p[0] = p[1] = p[2] = 0;
            continue;
        }
        if (a == 255) continue;
        p[0] = (uint8_t)std::min(255, (int)((int)p[0] * 255 + (a / 2)) / (int)a);
        p[1] = (uint8_t)std::min(255, (int)((int)p[1] * 255 + (a / 2)) / (int)a);
        p[2] = (uint8_t)std::min(255, (int)((int)p[2] * 255 + (a / 2)) / (int)a);
    }
}

// Copy a source file into user://lottie_cache/_mirror so it can be reached by
// an absolute path (needed when the asset lives inside the PCK, or on Web).
inline String mirror_file_to_user_cache(const String &src_path) {
    if (src_path.is_empty()) return String();
    PackedByteArray bytes = FileAccess::get_file_as_bytes(src_path);
    if (bytes.is_empty()) return String();
    const String root = String("user://lottie_cache/_mirror");
    String abs_root = ProjectSettings::get_singleton()->globalize_path(root);
    DirAccess::make_dir_recursive_absolute(abs_root);
    String base = src_path.get_file();
    String mirror_name = String::num_uint64((uint64_t)src_path.hash()) + String("_") + base;
    String mirror_rel = root.path_join(mirror_name);
    Ref<FileAccess> fo = FileAccess::open(mirror_rel, FileAccess::WRITE);
    if (fo.is_null()) return String();
    fo->store_buffer(bytes);
    fo->flush();
    fo->close();
    return mirror_rel;
}

// Extract a .lottie (zip) bundle into user://lottie_cache/<hash>/ and return
// the path to the JSON animation to load. `preferred_entry` optionally selects
// a specific animation id / inner path from a dotLottie bundle.
inline String extract_lottie_json_to_cache(const String &zip_path, const String &preferred_entry = String()) {
    Ref<ZIPReader> zr;
    zr.instantiate();
    if (zr.is_null()) {
        UtilityFunctions::printerr("Failed to open .lottie (zip): " + zip_path);
        return String();
    }

    String open_path = zip_path;
    Error zerr = zr->open(open_path);
    if (zerr != OK) {
        // On Web or when file is packed in PCK, mirror to user:// and try again.
        String mirrored = mirror_file_to_user_cache(zip_path);
        if (!mirrored.is_empty()) {
            zerr = zr->open(mirrored);
            if (zerr == OK) {
                open_path = mirrored;
            }
        }
    }
    if (zerr != OK) {
        UtilityFunctions::printerr("Failed to open .lottie (zip): " + zip_path);
        return String();
    }

    PackedStringArray files = zr->get_files();
    const String cache_root = String("user://lottie_cache");
    String abs_cache_root = ProjectSettings::get_singleton()->globalize_path(cache_root);
    DirAccess::make_dir_recursive_absolute(abs_cache_root);
    String hash_name = String::num_uint64((uint64_t)zip_path.hash());
    String cache_dir = cache_root.path_join(hash_name);
    String abs_cache_dir = ProjectSettings::get_singleton()->globalize_path(cache_dir);
    DirAccess::make_dir_recursive_absolute(abs_cache_dir);

    String json_inside;
    auto file_exists_in_zip = [&](const String &p) { for (int i = 0; i < files.size(); ++i) { if (files[i] == p) return true; } return false; };
    if (!preferred_entry.is_empty()) {
        if (file_exists_in_zip(preferred_entry)) json_inside = preferred_entry;
        if (json_inside.is_empty()) {
            String alt = String("animations/") + preferred_entry + ".json";
            if (file_exists_in_zip(alt)) json_inside = alt;
        }
        if (json_inside.is_empty()) {
            String needle = preferred_entry.to_lower();
            for (int i = 0; i < files.size(); i++) {
                String lf = files[i].to_lower();
                if (lf.ends_with(".json") && lf.find(needle) != -1) { json_inside = files[i]; break; }
            }
        }
    }
    if (json_inside.is_empty()) {
        for (int i = 0; i < files.size(); i++) {
            String f = files[i];
            String lf = f.to_lower();
            if (lf.ends_with(".json") && lf.begins_with("animations/")) { json_inside = f; break; }
        }
        if (json_inside.is_empty()) {
            for (int i = 0; i < files.size(); i++) {
                String f = files[i];
                String lf = f.to_lower();
                if (lf.ends_with("/data.json") || lf == "data.json") { json_inside = f; break; }
            }
        }
        if (json_inside.is_empty()) {
            for (int i = 0; i < files.size(); i++) {
                String f = files[i];
                String lf = f.to_lower();
                if (lf.ends_with(".json") && !lf.ends_with("manifest.json")) { json_inside = f; break; }
            }
        }
    }
    if (json_inside.is_empty()) {
        zr->close();
        UtilityFunctions::printerr(".lottie does not contain a JSON animation file");
        return String();
    }

    // Extract all files to the cache folder, so relative assets resolve.
    for (int i = 0; i < files.size(); i++) {
        String entry = files[i];
        if (entry.ends_with("/")) continue; // skip directory markers
        // Ensure parent directory exists
        String dest_rel = cache_dir.path_join(entry);
        String dest_abs = ProjectSettings::get_singleton()->globalize_path(dest_rel);
        String parent_abs = dest_abs.get_base_dir();
        DirAccess::make_dir_recursive_absolute(parent_abs);
        Ref<ZIPReader> zr3;
        zr3.instantiate();
        if (zr3->open(open_path) != OK) {
            UtilityFunctions::printerr("Failed to reopen .lottie for extract: " + open_path);
            return String();
        }
        PackedByteArray data = zr3->read_file(entry);
        zr3->close();
        Ref<FileAccess> fo = FileAccess::open(dest_rel, FileAccess::WRITE);
        if (fo.is_null()) {
            // Try to create parent again just in case
            DirAccess::make_dir_recursive_absolute(parent_abs);
            fo = FileAccess::open(dest_rel, FileAccess::WRITE);
        }
        if (fo.is_valid()) {
            fo->store_buffer(data);
            fo->flush();
            fo->close();
        }
    }

    // Return the chosen JSON inside the extracted cache dir
    String out_path = cache_dir.path_join(json_inside);
    return out_path;
}

} // namespace lottie

#endif // LOTTIE_COMMON_H
