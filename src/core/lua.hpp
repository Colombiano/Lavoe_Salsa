// Wrapper RAII move-only para lua_State (API C do Lua 5.4, sem sol2).
#pragma once

#include <stdexcept>
#include <string>
#include <utility>
#include <lua.hpp>

namespace lv {

class LuaState {
public:
    LuaState() : l_(luaL_newstate()) {
        if (!l_) throw std::runtime_error("luaL_newstate falhou");
        luaL_openlibs(l_);
    }

    ~LuaState() {
        if (l_) lua_close(l_);
    }

    LuaState(const LuaState&) = delete;
    LuaState& operator=(const LuaState&) = delete;

    LuaState(LuaState&& other) noexcept : l_(std::exchange(other.l_, nullptr)) {}

    LuaState& operator=(LuaState&& other) noexcept {
        if (this != &other) {
            if (l_) lua_close(l_);
            l_ = std::exchange(other.l_, nullptr);
        }
        return *this;
    }

    [[nodiscard]] lua_State* get() const { return l_; }

    // Executa um arquivo; lanca std::runtime_error com a mensagem do Lua.
    void dofile(const std::string& path) {
        if (luaL_dofile(l_, path.c_str()) != LUA_OK) {
            std::string err = lua_tostring(l_, -1);
            lua_pop(l_, 1);
            throw std::runtime_error("erro no config '" + path + "': " + err);
        }
    }

private:
    lua_State* l_ = nullptr;
};

} // namespace lv
