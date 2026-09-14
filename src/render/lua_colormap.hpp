// ColorMap Lua: chama a funcao `bar_color(i, nivel, t)` do config a cada
// barra (uma cor plana por barra). Tem prioridade sobre o gradiente.
#pragma once

#include <lua.hpp>
#include "dsp/color.hpp"
#include "dsp/gradient.hpp"

namespace lv {

class LuaColorMap {
public:
    static constexpr bool kPerStrip = false;

    LuaColorMap(lua_State* L, int ref) : L_(L), ref_(ref) {}

    [[nodiscard]] Color bar_color(int i, float level, float t) const {
        if (!L_ || ref_ == LUA_NOREF) return Color{1.f, 1.f, 1.f};
        lua_rawgeti(L_, LUA_REGISTRYINDEX, ref_);
        lua_pushinteger(L_, static_cast<lua_Integer>(i) + 1);  // 1-based p/ Lua
        lua_pushnumber(L_, static_cast<lua_Number>(level));
        lua_pushnumber(L_, static_cast<lua_Number>(t));
        if (lua_pcall(L_, 3, 1, 0) != LUA_OK) {
            lua_pop(L_, 1);  // mensagem de erro
            return Color{1.f, 1.f, 1.f};
        }
        Color c{1.f, 1.f, 1.f};
        if (lua_istable(L_, -1)) {
            auto channel = [&](const char* k) {
                lua_getfield(L_, -1, k);
                const float v = static_cast<float>(lua_tonumber(L_, -1));
                lua_pop(L_, 1);
                return v;
            };
            c = Color{channel("r"), channel("g"), channel("b")};
        } else if (lua_isstring(L_, -1)) {
            if (auto hex = try_hex_rgb(lua_tostring(L_, -1)))
                c = from_hex(*hex);
        }
        lua_pop(L_, 1);
        return c;
    }

private:
    lua_State* L_;
    int ref_;
};

} // namespace lv
