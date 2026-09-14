#include "core/cli.hpp"
#include "core/version.hpp"
#include <cstdio>
#include <cstring>
#include <stdexcept>

namespace lv {

static const char* kHelp =
    "Lavoe Salsa - visualizador de audio overlay (homenagem a Hector Lavoe)\n"
    "\n"
    "Uso: lavoe-salsa [opcoes]\n"
    "\n"
    "  --config <caminho>   usa outro arquivo de configuracao Lua\n"
    "  --dump               imprime as alturas das barras como CSV por frame\n"
    "                       (para piping: lavoe-salsa --dump | ferramenta)\n"
    "  --help               mostra esta ajuda\n"
    "  --version            mostra a versao\n"
    "\n"
    "Na primeira execucao, o arquivo ~/.config/lavoe-salsa/config.lua e\n"
    "gerado com os padroes comentados.\n";

std::optional<int> parse_cli(int argc, char** argv, Options& out) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        auto need_value = [&](const char* name) -> std::string {
            if (i + 1 >= argc)
                throw std::runtime_error(std::string("opcao ") + name +
                                         " precisa de um valor");
            return argv[++i];
        };
        if (arg == "--help" || arg == "-h") {
            out.help = true;
        } else if (arg == "--version" || arg == "-v") {
            out.version = true;
        } else if (arg == "--dump") {
            out.dump = true;
        } else if (arg == "--config") {
            out.config = need_value("--config");
        } else if (arg.rfind("--config=", 0) == 0) {
            out.config = arg.substr(std::strlen("--config="));
        } else {
            throw std::runtime_error("opcao desconhecida: " + arg +
                                     " (veja --help)");
        }
    }
    if (out.help) {
        std::fputs(kHelp, stdout);
        return 0;
    }
    if (out.version) {
        std::fprintf(stdout, "%s %s\n", kAppName, kVersion);
        return 0;
    }
    return std::nullopt;
}

} // namespace lv
