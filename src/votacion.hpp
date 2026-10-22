/*
Votación entre las copias del mensaje (O2). Equivale a prototipo/votacion.py.
*/

#ifndef VOTACION_HPP
#define VOTACION_HPP

#include <cstddef>
#include <optional>

#include "trama.hpp"

namespace votacion {

constexpr std::size_t PERIODO_MIN = 48;                  // 40 bits de sobrecarga y 1 byte
constexpr std::size_t PERIODO_MAX = 7 * (40 + 8 * 255);  // 7 registros de 255 bytes
constexpr std::size_t MIN_COPIAS = 3;
constexpr int DERIVA_MAX = 4;
constexpr std::size_t BLOQUE = 64;
constexpr long long UMBRAL_MILESIMAS = 750;  // coincidencia mínima en el período
constexpr long long MARGEN_MILESIMAS = 20;   // se prefiere el menor período a esta distancia
constexpr int PASADAS = 2;

// Período del mensaje, o vacío si no hay al menos MIN_COPIAS copias claras.
std::optional<std::size_t> estimar_periodo(const trama::Bits& bits);

// Una copia del mensaje votada bit a bit, o vacío si no se pudo estimar el
// período. La copia empieza donde empieza bits, no necesariamente en un registro.
std::optional<trama::Bits> votar_copias(const trama::Bits& bits);

}  // namespace votacion

#endif
