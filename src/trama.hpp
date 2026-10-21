/*
Tipos comunes de la trama (equivale a prototipo/trama.py).

No contiene lógica, solo define el formato de la comunicación que tendrá lugar entre
los diferentes módulos.

Un formato de trama convierte metadatos en los bits de una copia del mensaje
(codificar) y lee los registros que hay en una secuencia de bits (leer). El único
formato es "registros" (trama_registros.hpp).
*/

#ifndef TRAMA_HPP
#define TRAMA_HPP

#include <map>
#include <string>
#include <vector>

namespace trama {

using Bits = std::vector<int>;                 // un bit (0 o 1) por ventana
using Metadatos = std::map<int, std::string>;  // categoría -> texto en bytes UTF-8

enum class Tipo {
    valido,
    desconocido,  // se leyó bien, pero la categoría o el texto no son válidos
    danado,       // la comprobación de errores del formato falló
};

struct Lectura {
    Tipo tipo;
    int categoria;
    std::string datos;
};

inline bool operator==(const Lectura& a, const Lectura& b) {
    return a.tipo == b.tipo && a.categoria == b.categoria && a.datos == b.datos;
}

}  // namespace trama

#endif
