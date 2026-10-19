/*
Validación de UTF-8 según la tabla 3-7 del estándar Unicode: rechaza secuencias
sobrelargas, sustitutos y valores mayores que U+10FFFF. No tiene un equivalente
del lado del prototipo porque Python realiza esta validación de forma interna.
La usan la trama (texto válido), la notificación (los "?") y la línea de comandos (metadatos).

*/

#ifndef UTF8_HPP
#define UTF8_HPP

#include <cstddef>
#include <string>

namespace utf8 {

// Largo de la secuencia UTF-8 válida que empieza en datos[i], o 0.
inline std::size_t largo_secuencia(const std::string& datos, std::size_t i) {
    struct Rango {
        unsigned char bajo, alto;
    };
    const auto b = static_cast<unsigned char>(datos[i]);
    if (b < 0x80) return 1;

    Rango rangos[3];
    std::size_t n = 0;
    if (b >= 0xC2 && b <= 0xDF) {
        rangos[n++] = {0x80, 0xBF};
    } else if (b == 0xE0) {
        rangos[n++] = {0xA0, 0xBF};
        rangos[n++] = {0x80, 0xBF};
    } else if ((b >= 0xE1 && b <= 0xEC) || b == 0xEE || b == 0xEF) {
        rangos[n++] = {0x80, 0xBF};
        rangos[n++] = {0x80, 0xBF};
    } else if (b == 0xED) {
        rangos[n++] = {0x80, 0x9F};
        rangos[n++] = {0x80, 0xBF};
    } else if (b == 0xF0) {
        rangos[n++] = {0x90, 0xBF};
        rangos[n++] = {0x80, 0xBF};
        rangos[n++] = {0x80, 0xBF};
    } else if (b >= 0xF1 && b <= 0xF3) {
        rangos[n++] = {0x80, 0xBF};
        rangos[n++] = {0x80, 0xBF};
        rangos[n++] = {0x80, 0xBF};
    } else if (b == 0xF4) {
        rangos[n++] = {0x80, 0x8F};
        rangos[n++] = {0x80, 0xBF};
        rangos[n++] = {0x80, 0xBF};
    } else {
        return 0;
    }
    if (i + n >= datos.size()) return 0;  // secuencia truncada
    for (std::size_t k = 0; k < n; ++k) {
        const auto c = static_cast<unsigned char>(datos[i + 1 + k]);
        if (c < rangos[k].bajo || c > rangos[k].alto) return 0;
    }
    return 1 + n;
}

inline bool valido(const std::string& datos) {
    for (std::size_t i = 0; i < datos.size();) {
        const std::size_t largo = largo_secuencia(datos, i);
        if (largo == 0) return false;
        i += largo;
    }
    return true;
}

}  // namespace utf8

#endif
