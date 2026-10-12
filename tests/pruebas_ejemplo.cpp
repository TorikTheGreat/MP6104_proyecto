// Ejemplo de uso de tests/pruebas.hpp y, a la vez, pruebas de la propia
// infraestructura. Sirve de plantilla para tests/pruebas_<módulo>.cpp.

#include <complex>
#include <cstdint>
#include <string>
#include <vector>

#include "pruebas.hpp"

PRUEBA(ejemplo_verificaciones_basicas) {
    const std::vector<int> v = {1, 2, 3};
    REQUERIR(v.size() == 3);  // si falla, no se sigue a v[2]
    VERIFICAR(v[2] > v[0]);
    VERIFICAR_IGUAL(v.size(), 3);  // size_t contra int, sin -Wsign-compare
    VERIFICAR_IGUAL(v, (std::vector<int>{1, 2, 3}));
    VERIFICAR_CERCA(0.1 + 0.2, 0.3, 1e-12);
    VERIFICAR_CERCA(std::complex<double>(1, 1), std::complex<double>(1, 1 + 1e-13), 1e-12);
    VERIFICAR_IGUAL(std::string("Canción"), "Canción");
}

PRUEBA(infraestructura_iguales_con_y_sin_signo) {
    VERIFICAR(pruebas::iguales(3u, 3));
    VERIFICAR(!pruebas::iguales(-1, 4294967295u));  // -1 no es igual a 2³² − 1
    VERIFICAR(!pruebas::iguales(static_cast<std::size_t>(-1), -1));
    VERIFICAR(pruebas::iguales(std::uint8_t{0xB2}, 178));
    VERIFICAR(pruebas::iguales(static_cast<unsigned short>(7), 7u));
    VERIFICAR(pruebas::iguales(-5L, -5));
}

PRUEBA(infraestructura_iguales_cadenas_y_vectores) {
    const char a[] = "hola";
    const char* b = "hola";
    VERIFICAR(pruebas::iguales(a, b));  // compara el texto, no los punteros
    VERIFICAR(!pruebas::iguales("hola", "holas"));
    VERIFICAR(!pruebas::iguales(std::vector<int>{1, 2}, std::vector<int>{1, 2, 3}));
    VERIFICAR(pruebas::iguales(std::vector<std::uint8_t>{1, 0}, std::vector<std::uint8_t>{1, 0}));
}

PRUEBA(infraestructura_mensajes_de_diferencia) {
    VERIFICAR_IGUAL(pruebas::diferencia(std::vector<int>{1, 2}, std::vector<int>{1, 2, 3}),
                    "tamaño 2, se esperaba 3");
    VERIFICAR_IGUAL(pruebas::diferencia(std::vector<int>{1, 5, 3}, std::vector<int>{1, 2, 3}),
                    "en [1] se obtuvo 5, se esperaba 2");
    VERIFICAR_IGUAL(pruebas::mostrar(std::uint8_t{0xB2}), "178");
    VERIFICAR_IGUAL(pruebas::mostrar(true), "true");
}

PRUEBA(infraestructura_cerca_rechaza_nan) {
    std::string detalle;
    VERIFICAR(!pruebas::cerca(std::nan(""), 0.0, 1.0, detalle));
    VERIFICAR(!pruebas::cerca(1.0, 1.1, 0.01, detalle));
    VERIFICAR(pruebas::cerca(1.0, 1.005, 0.01, detalle));
}

PRUEBA(infraestructura_registra_las_fallas) {
    // Una verificación fallida deja un mensaje con archivo y línea; se vacía
    // aquí mismo para que esta prueba no cuente como fallida.
    const std::size_t antes = pruebas::fallos().size();
    VERIFICAR_IGUAL(1 + 1, 3);
    const bool registrada = pruebas::fallos().size() == antes + 1 &&
                            pruebas::fallos().back().find("pruebas_ejemplo.cpp:") !=
                                std::string::npos &&
                            pruebas::fallos().back().find("se obtuvo 2, se esperaba 3") !=
                                std::string::npos;
    pruebas::fallos().resize(antes);
    VERIFICAR(registrada);
}
