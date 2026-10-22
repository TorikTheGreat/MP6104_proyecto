// ecoesteg: esconde metadatos en un WAV con enmascaramiento con eco, o los recupera.
// Uso: ecoesteg --ayuda

#include <iostream>
#include <string>
#include <vector>

#include "aplicacion.hpp"

int main(int argc, char** argv) {
    const std::vector<std::string> args(argv + 1, argv + argc);
    return aplicacion::ejecutar(args, std::cout, std::cerr, aplicacion::bloques_reales());
}
