// Parsing da linha de comando.
#pragma once

#include <optional>
#include <string>

namespace lv {

struct Options {
    std::optional<std::string> config;  // --config <caminho>
    bool dump = false;                  // --dump: CSV no stdout por frame
    bool help = false;                  // --help
    bool version = false;               // --version
};

// Lanca std::runtime_error em opcao invalida; std::nullopt indica sucesso.
std::optional<int> parse_cli(int argc, char** argv, Options& out);

} // namespace lv
