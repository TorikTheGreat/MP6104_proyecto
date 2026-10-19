/*
Formato de la trama (el frame) "registros": un registro por metadato. (equivale a
prototipo/trama_registros.py)

Formato:
preámbulo   8 bits    0xB2
categoría   8 bits    1-7
longitud    8 bits    n, bytes del texto en UTF-8 (1-255)
datos       8n bits   el texto en UTF-8
CRC-16      16 bits   CRC-16/CCITT-FALSE sobre categoría, longitud y datos

Cada byte va con el bit más significativo primero. Una copia del mensaje son los
registros en orden de categoría. tests/pruebas_trama_registros.cpp compara con los
vectores de prototipo/vectores_metadatos.py.
*/

#ifndef TRAMA_REGISTROS_HPP
#define TRAMA_REGISTROS_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "trama.hpp"

namespace trama_registros {

constexpr int PREAMBULO = 0xB2;
constexpr std::size_t BITS_SOBRECARGA = 40;
constexpr std::size_t LONGITUD_MAX = 255;

// CRC-16/CCITT-FALSE: polinomio 0x1021, valor inicial 0xFFFF.
// crc16("123456789") == 0x29B1.
std::uint16_t crc16(const std::string& datos);

trama::Bits bits_de_bytes(const std::string& datos);

// Bits de una copia del mensaje. Tira std::invalid_argument si no hay metadatos,
// si una categoría no es 1-7 o si un texto no tiene de 1 a 255 bytes.
trama::Bits codificar(const trama::Metadatos& metadatos);
/*
Registros leídos en bits, en orden, con repetidos.

Donde terminó el último registro con CRC correcto se acepta un preámbulo con
hasta TOLERANCIA_PREAMBULO bits distintos; en cualquier otra posición, solo uno
exacto. Si el registro no se confirma, se busca desde el bit siguiente: así un
bit borrado o insertado cuesta un registro. Un CRC incorrecto se anota como
dañado solo si el registro empezaba donde terminó el anterior; los demás suelen
ser falsos preámbulos dentro de los datos. Lanza std::invalid_argument si algún
bit no es 0 o 1.*/
std::vector<trama::Lectura> leer(const trama::Bits& bits);

}  // namespace trama_registros

#endif
