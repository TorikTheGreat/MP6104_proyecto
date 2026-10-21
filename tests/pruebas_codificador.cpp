// Pruebas del modulo A: codificacion de eco y Overlap-Add.
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include "codificador.hpp"
#include "pruebas.hpp"

PRUEBA(codificador_bit_cero_impulso) {
    const std::vector<int16_t> x = {1000, 0, 0, 0, 0, 0, 0, 0};
    const auto y = codificar_eco(x, "0", 2, 3, 0.2, 0.4, 4);
    const std::vector<double> esperado = {1000, 0, 200, 0, 0, 0, 0, 0, 0, 0, 0};
    VERIFICAR_IGUAL(y, esperado);
}

PRUEBA(codificador_bit_uno_impulso) {
    const std::vector<int16_t> x = {1000, 0, 0, 0, 0, 0, 0, 0};
    const auto y = codificar_eco(x, "1", 2, 3, 0.2, 0.4, 4);
    const std::vector<double> esperado = {1000, 0, 0, 400, 0, 0, 0, 0, 0, 0, 0};
    VERIFICAR_IGUAL(y, esperado);
}

PRUEBA(codificador_overlap_add_suma_en_limite) {
    // Ventana 0: impulso en indice 3, eco con D0=2 llega a indice 5.
    // Ventana 1: muestra original en indice 5 vale 50.
    // Resultado en indice 5: 50 + 0.2*1000 = 250.
    const std::vector<int16_t> x = {0, 0, 0, 1000, 0, 50, 0, 0};
    const auto y = codificar_eco(x, "01", 2, 3, 0.2, 0.4, 4);
    VERIFICAR_IGUAL(y.size(), 11);
    VERIFICAR_CERCA(y[5], 250.0, 1e-12);
    VERIFICAR_CERCA(y[8], 20.0, 1e-12); // eco de x[5] con D1=3
    VERIFICAR_CERCA(y[3], 1000.0, 1e-12);
}

PRUEBA(codificador_conserva_original_fuera_de_ecos) {
    const std::vector<int16_t> x = {10, -20, 30, -40, 50, -60, 70, -80};
    const auto y = codificar_eco(x, "0", 2, 3, 0.0, 0.4, 4);
    VERIFICAR_IGUAL(y.size(), 11);
    for (std::size_t i = 0; i < x.size(); ++i)
        VERIFICAR_CERCA(y[i], static_cast<double>(x[i]), 1e-12);
    for (std::size_t i = x.size(); i < y.size(); ++i)
        VERIFICAR_CERCA(y[i], 0.0, 1e-12);
}

PRUEBA(codificador_rechaza_parametros_invalidos) {
    const std::vector<int16_t> x(8, 100);
    auto falla = [&](const std::string& bits, std::size_t d0, std::size_t d1,
                     double a0, double a1, std::size_t ventana) {
        try {
            (void)codificar_eco(x, bits, d0, d1, a0, a1, ventana);
            return false;
        } catch (const std::runtime_error&) {
            return true;
        }
    };
    VERIFICAR(falla("0", 0, 3, 0.2, 0.2, 4));
    VERIFICAR(falla("0", 2, 2, 0.2, 0.2, 4));
    VERIFICAR(falla("0", 2, 3, 0.2, 0.2, 3));
    VERIFICAR(falla("0", 2, 3, -0.1, 0.2, 4));
    VERIFICAR(falla("0", 2, 3, 0.2, 1.0, 4));
    VERIFICAR(falla("", 2, 3, 0.2, 0.2, 4));
    VERIFICAR(falla("2", 2, 3, 0.2, 0.2, 4));
    VERIFICAR(falla("010", 2, 3, 0.2, 0.2, 4));
}
