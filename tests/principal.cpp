// main de bin/pruebas: corre las pruebas anotadas con PRUEBA (tests/pruebas.hpp).
//
// Uso: bin/pruebas [filtro]
//   Sin argumentos corre todas; con un filtro, solo las que lo contienen en su
//   nombre (bin/pruebas fft). Devuelve 0 si todas pasan y 1 si alguna falla, si
//   hay nombres repetidos o si el filtro no coincide con ninguna; make test y la
//   CI se guían por ese código.

#include <exception>
#include <iostream>
#include <set>
#include <string>

#include "pruebas.hpp"

int main(int argc, char** argv) {
    if (argc > 2) {
        std::cerr << "uso: " << argv[0] << " [filtro]\n";
        return 2;
    }
    const std::string filtro = argc == 2 ? argv[1] : "";

    std::set<std::string> nombres;
    for (const auto& p : pruebas::registro()) {
        if (!nombres.insert(p.nombre).second) {
            std::cout << "ERROR: hay dos pruebas llamadas «" << p.nombre << "»\n";
            return 1;
        }
    }

    int corridas = 0;
    int fallidas = 0;
    for (const auto& p : pruebas::registro()) {
        if (std::string(p.nombre).find(filtro) == std::string::npos) continue;
        ++corridas;
        pruebas::fallos().clear();
        try {
            p.funcion();
        } catch (const std::exception& e) {
            pruebas::fallos().push_back(std::string("excepción no capturada: ") + e.what());
        } catch (...) {
            pruebas::fallos().push_back("excepción no capturada de tipo desconocido");
        }
        if (pruebas::fallos().empty()) {
            std::cout << "ok     " << p.nombre << "\n";
        } else {
            ++fallidas;
            std::cout << "FALLA  " << p.nombre << "\n";
            for (const auto& f : pruebas::fallos()) std::cout << "         " << f << "\n";
        }
    }

    if (corridas == 0 && !filtro.empty()) {
        std::cout << "ERROR: ninguna prueba contiene «" << filtro << "» en su nombre\n";
        return 1;
    }
    std::cout << "\n" << corridas << (corridas == 1 ? " prueba, " : " pruebas, ")
              << fallidas << (fallidas == 1 ? " fallida" : " fallidas") << "\n";
    return fallidas == 0 ? 0 : 1;
}
