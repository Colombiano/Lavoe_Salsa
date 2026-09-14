// Geometria de monitores e posicionamento da janela.
//
// Query de monitores em runtime: tenta Xinerama via dlopen (sem dependencia
// de -dev em tempo de build), cai para parse de `xrandr --query`, e por
// ultimo para "um monitor" cobrindo a tela inteira.
#pragma once

#include <string>
#include <vector>
#include "core/config.hpp"

typedef struct _XDisplay Display;

namespace lv {

struct Rect {
    int x = 0, y = 0, w = 0, h = 0;
};

std::vector<Rect> query_monitors(Display* dpy);

// Centraliza na base do monitor escolhido, respeitando a margem inferior.
// Se cfg.x e cfg.y estiverem ambos definidos, usa posicao absoluta.
Rect compute_window_rect(const Config& cfg, const std::vector<Rect>& monitors);

} // namespace lv
