// Cor RGB em ponto flutuante + parsing de "#rrggbb" (compile-time e runtime).
#pragma once

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string_view>

namespace lv {

struct Color {
    float r{0.f}, g{0.f}, b{0.f};

    constexpr Color() = default;
    constexpr Color(float r_, float g_, float b_) : r(r_), g(g_), b(b_) {}
};

// ---------------------------------------------------------------------------
// Parsing hexadecimal em tempo de compilacao (consteval).
// ---------------------------------------------------------------------------

consteval std::uint8_t hex_digit(char c) {
    if (c >= '0' && c <= '9') return static_cast<std::uint8_t>(c - '0');
    if (c >= 'a' && c <= 'f') return static_cast<std::uint8_t>(c - 'a' + 10);
    if (c >= 'A' && c <= 'F') return static_cast<std::uint8_t>(c - 'A' + 10);
    throw std::invalid_argument("digito hex invalido");
}

// Aceita "#rrggbb" (7 chars) e retorna 0xRRGGBB.
consteval std::uint32_t hex_rgb(std::string_view sv) {
    if (sv.size() != 7 || sv.front() != '#')
        throw std::invalid_argument("esperado #rrggbb");
    std::uint32_t v = 0;
    for (std::size_t i = 1; i < 7; ++i)
        v = (v << 4) | hex_digit(sv[i]);
    return v;
}

constexpr Color from_hex(std::uint32_t v) {
    return Color{static_cast<float>((v >> 16) & 0xff) / 255.f,
                 static_cast<float>((v >> 8) & 0xff) / 255.f,
                 static_cast<float>(v & 0xff) / 255.f};
}

// Literal "_rgb": "#cc241d"_rgb -> Color, avaliado em compile-time.
consteval Color operator""_rgb(const char* s, std::size_t n) {
    return from_hex(hex_rgb(std::string_view{s, n}));
}

// ---------------------------------------------------------------------------
// Parsing em runtime (para stops vindos do config Lua).
// ---------------------------------------------------------------------------

inline std::optional<std::uint32_t> try_hex_rgb(std::string_view sv) {
    if (sv.size() != 7 || sv.front() != '#') return std::nullopt;
    std::uint32_t v = 0;
    for (std::size_t i = 1; i < 7; ++i) {
        char c = sv[i];
        std::uint8_t d;
        if (c >= '0' && c <= '9') d = static_cast<std::uint8_t>(c - '0');
        else if (c >= 'a' && c <= 'f') d = static_cast<std::uint8_t>(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') d = static_cast<std::uint8_t>(c - 'A' + 10);
        else return std::nullopt;
        v = (v << 4) | d;
    }
    return v;
}

} // namespace lv
