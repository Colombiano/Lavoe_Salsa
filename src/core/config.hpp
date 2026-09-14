// Configuracao do Lavoe Salsa, carregada do arquivo Lua.
#pragma once

#include <lua.hpp>

#include <optional>
#include <string>
#include <vector>
#include "dsp/color.hpp"
#include "dsp/gradient.hpp"
#include "dsp/smoothing.hpp"

namespace lv {

struct Config {
    // geometria
    int monitor = 0;
    std::optional<int> x, y;
    int w = 1100, h = 280;
    int bottom_margin = 70;
    int padding = 12;

    // barras
    int bars = 56;
    int bar_width = 6;
    int bar_gap = 3;

    // loop
    int fps = 60;
    float sample_rate = 48000.f;

    // aparencia
    std::vector<GradientStop> gradient;  // vazio => LUT compile-time padrao
    SmoothingParams smoothing;

    // hook Lua opcional (ref no registry); LUA_NOREF se ausente
    int bar_color_ref = LUA_NOREF;

    std::string path;
};

// Carrega o config Lua. Se o arquivo nao existir, gera o padrao comentado
// em ~/.config/lavoe-salsa/config.lua (ou no caminho --config informado).
// `lua` precisa continuar vivo enquanto o Config existir (bar_color_ref).
Config load_config(class LuaState& lua, const std::optional<std::string>& cli_path);

} // namespace lv
