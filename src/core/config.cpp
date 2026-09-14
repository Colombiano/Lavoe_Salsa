#include "core/config.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <lua.hpp>

#include "core/lua.hpp"
#include "default_config.hpp"

namespace fs = std::filesystem;

namespace lv {

namespace {

// Leitor de campos sobre uma tabela Lua (indice absoluto no stack).
struct TableView {
    lua_State* L;
    int idx;  // indice absoluto

    static TableView global(lua_State* L) {
        lua_pushglobaltable(L);
        return TableView{L, lua_gettop(L)};
    }

    std::optional<TableView> table(const char* key) const {
        lua_getfield(L, idx, key);
        if (!lua_istable(L, -1)) {
            lua_pop(L, 1);
            return std::nullopt;
        }
        TableView v{L, lua_absindex(L, -1)};
        return v;
    }

    std::optional<double> num(const char* key) const {
        lua_getfield(L, idx, key);
        std::optional<double> r;
        if (lua_isnumber(L, -1)) r = lua_tonumber(L, -1);
        lua_pop(L, 1);
        return r;
    }

    std::optional<std::string> str(const char* key) const {
        lua_getfield(L, idx, key);
        std::optional<std::string> r;
        if (lua_isstring(L, -1)) r = std::string{lua_tostring(L, -1)};
        lua_pop(L, 1);
        return r;
    }
};

void parse_geometry(const TableView& t, Config& c) {
    if (auto v = t.num("monitor")) c.monitor = static_cast<int>(*v);
    if (auto v = t.num("x")) c.x = static_cast<int>(*v);
    if (auto v = t.num("y")) c.y = static_cast<int>(*v);
    if (auto v = t.num("w")) c.w = static_cast<int>(*v);
    if (auto v = t.num("h")) c.h = static_cast<int>(*v);
    if (auto v = t.num("margem_base")) c.bottom_margin = static_cast<int>(*v);
    if (auto v = t.num("margem_lados")) c.padding = static_cast<int>(*v);
}

void parse_gradient(lua_State* L, int table_idx, Config& c) {
    c.gradient.clear();
    const std::size_t n = luaL_len(L, table_idx);
    for (std::size_t i = 1; i <= n; ++i) {
        lua_rawgeti(L, table_idx, static_cast<lua_Integer>(i));
        if (!lua_istable(L, -1)) {
            lua_pop(L, 1);
            continue;
        }
        lua_rawgeti(L, -1, 1);  // posicao
        const float pos = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);
        GradientStop stop{pos, Color{}};
        bool ok = false;
        lua_rawgeti(L, -1, 2);  // cor
        if (lua_isstring(L, -1)) {
            if (auto hex = try_hex_rgb(lua_tostring(L, -1))) {
                stop.color = from_hex(*hex);
                ok = true;
            }
        } else if (lua_istable(L, -1)) {
            auto channel = [&](const char* k) {
                lua_getfield(L, -1, k);
                const float v = static_cast<float>(lua_tonumber(L, -1));
                lua_pop(L, 1);
                return v;
            };
            stop.color = Color{channel("r"), channel("g"), channel("b")};
            ok = true;
        }
        lua_pop(L, 1);  // cor
        lua_pop(L, 1);  // entrada
        if (ok) c.gradient.push_back(stop);
    }
}

void parse_smoothing(const TableView& t, Config& c) {
    if (auto v = t.num("ataque")) c.smoothing.attack = static_cast<float>(*v);
    if (auto v = t.num("queda")) c.smoothing.decay = static_cast<float>(*v);
    if (auto v = t.num("piso_ruido"))
        c.smoothing.noise_floor = static_cast<float>(*v);
    if (auto v = t.num("gama")) c.smoothing.gamma = static_cast<float>(*v);
    if (auto v = t.num("piso_db")) c.smoothing.db_floor = static_cast<float>(*v);
}

std::string default_config_path() {
    if (const char* xdg = std::getenv("XDG_CONFIG_HOME"); xdg && *xdg)
        return (fs::path{xdg} / "lavoe-salsa" / "config.lua").string();
    if (const char* home = std::getenv("HOME"); home && *home)
        return (fs::path{home} / ".config" / "lavoe-salsa" / "config.lua").string();
    throw std::runtime_error("HOME nao definido; use --config <caminho>");
}

void write_default_config(const std::string& path) {
    std::error_code ec;
    fs::create_directories(fs::path{path}.parent_path(), ec);
    std::ofstream out{path, std::ios::trunc};
    if (!out) throw std::runtime_error("nao consegui criar " + path);
    out << kEmbeddedDefaultConfig;
}

} // namespace

Config load_config(LuaState& lua, const std::optional<std::string>& cli_path) {
    Config cfg;
    cfg.path = cli_path.value_or(default_config_path());

    if (!fs::exists(cfg.path)) {
        write_default_config(cfg.path);
        std::fprintf(stderr, "lavoe-salsa: config gerado em %s\n", cfg.path.c_str());
    }

    lua.dofile(cfg.path);
    lua_State* L = lua.get();

    // Aceita tanto `return {...}` quanto um arquivo que so define globais.
    TableView root = lua_istable(L, -1)
                         ? TableView{L, lua_absindex(L, -1)}
                         : TableView::global(L);

    if (auto geo = root.table("geometry")) parse_geometry(*geo, cfg);
    if (auto v = root.num("barras")) cfg.bars = static_cast<int>(*v);
    if (auto v = root.num("largura_barra")) cfg.bar_width = static_cast<int>(*v);
    if (auto v = root.num("espacamento")) cfg.bar_gap = static_cast<int>(*v);
    if (auto v = root.num("fps")) cfg.fps = static_cast<int>(*v);
    if (auto v = root.num("taxa_amostragem")) cfg.sample_rate = static_cast<float>(*v);
    if (auto grad = root.table("gradiente")) {
        lua_pushvalue(L, -1);
        parse_gradient(L, lua_gettop(L), cfg);
        lua_pop(L, 1);
    }
    if (auto sm = root.table("suavizacao")) parse_smoothing(*sm, cfg);

    // hook bar_color (opcional): funcao no campo ou global
    lua_getfield(L, root.idx, "bar_color");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 1);
        lua_getglobal(L, "bar_color");
    }
    if (lua_isfunction(L, -1)) {
        cfg.bar_color_ref = luaL_ref(L, LUA_REGISTRYINDEX);
        std::fprintf(stderr, "lavoe-salsa: hook bar_color ativo\n");
    } else {
        lua_pop(L, 1);
    }

    // sanity
    cfg.bars = std::clamp(cfg.bars, 8, 256);
    cfg.w = std::max(cfg.w, 100);
    cfg.h = std::max(cfg.h, 60);
    cfg.bar_width = std::clamp(cfg.bar_width, 1, 64);
    cfg.bar_gap = std::clamp(cfg.bar_gap, 0, 64);
    cfg.fps = std::clamp(cfg.fps, 1, 240);
    return cfg;
}

} // namespace lv
