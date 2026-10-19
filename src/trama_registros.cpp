#include "trama_registros.hpp"

#include <bitset>
#include <stdexcept>

#include "parametros.hpp"
#include "utf8.hpp"

namespace trama_registros {

using trama::Bits;
using trama::Lectura;
using trama::Tipo;


/*Equivalente a prototipo/trama_registros.py.*/


namespace {

// Byte formado por bits[i .. i+7], el más significativo primero.
int byte_en(const Bits& bits, std::size_t i) {
    int valor = 0;
    for (std::size_t k = i; k < i + 8; ++k) valor = (valor << 1) | bits[k];
    return valor;
}

int bits_distintos(int a, int b) {
    return static_cast<int>(std::bitset<8>(static_cast<unsigned>(a ^ b)).count());
}

}  // namespace

std::uint16_t crc16(const std::string& datos) {
    std::uint16_t crc = 0xFFFF;
    for (const char c : datos) {
        crc ^= static_cast<std::uint16_t>(static_cast<unsigned char>(c) << 8);
        for (int k = 0; k < 8; ++k)
            crc = (crc & 0x8000) ? static_cast<std::uint16_t>((crc << 1) ^ 0x1021)
                                 : static_cast<std::uint16_t>(crc << 1);
    }
    return crc;
}

Bits bits_de_bytes(const std::string& datos) {
    Bits bits;
    bits.reserve(8 * datos.size());
    for (const char c : datos) {
        const auto byte = static_cast<unsigned char>(c);
        for (int k = 7; k >= 0; --k) bits.push_back((byte >> k) & 1);
    }
    return bits;
}

Bits codificar(const trama::Metadatos& metadatos) {
    if (metadatos.empty()) throw std::invalid_argument("no hay metadatos que codificar");
    Bits bits;
    for (const auto& [categoria, datos] : metadatos) {  // std::map: orden de categoría
        const char* nombre = parametros::nombre_categoria(categoria);
        if (nombre == nullptr)
            throw std::invalid_argument("categoría inválida: " + std::to_string(categoria));
        if (datos.empty() || datos.size() > LONGITUD_MAX)
            throw std::invalid_argument(std::string(nombre) + ": " +
                                        std::to_string(datos.size()) +
                                        " bytes (válido: 1 a 255)");
        std::string cuerpo;
        cuerpo += static_cast<char>(categoria);
        cuerpo += static_cast<char>(datos.size());
        cuerpo += datos;
        const std::uint16_t crc = crc16(cuerpo);
        const std::string registro = static_cast<char>(PREAMBULO) + cuerpo +
                                     static_cast<char>(crc >> 8) +
                                     static_cast<char>(crc & 0xFF);
        const Bits b = bits_de_bytes(registro);
        bits.insert(bits.end(), b.begin(), b.end());
    }
    return bits;
}

std::vector<Lectura> leer(const Bits& bits) {
    for (const int b : bits)
        if (b != 0 && b != 1)
            throw std::invalid_argument("bit distinto de 0 y 1: " + std::to_string(b));

    const std::size_t n = bits.size();
    std::vector<Lectura> lecturas;
    std::size_t esperado = 0;  // fin del último registro con CRC correcto
    std::size_t inicio = 0;
    while (inicio + BITS_SOBRECARGA + 8 <= n) {
        const bool en_secuencia = inicio == esperado;
        const int tolerancia = en_secuencia ? parametros::TOLERANCIA_PREAMBULO : 0;
        if (bits_distintos(byte_en(bits, inicio), PREAMBULO) > tolerancia) {
            ++inicio;
            continue;
        }
        const int categoria = byte_en(bits, inicio + 8);
        const auto longitud = static_cast<std::size_t>(byte_en(bits, inicio + 16));
        const std::size_t fin = inicio + BITS_SOBRECARGA + 8 * longitud;
        if (longitud == 0 || fin > n) {
            ++inicio;
            continue;
        }
        std::string cuerpo;
        for (std::size_t k = 0; k < 2 + longitud; ++k)
            cuerpo += static_cast<char>(byte_en(bits, inicio + 8 + 8 * k));
        const int crc = (byte_en(bits, fin - 16) << 8) | byte_en(bits, fin - 8);
        const std::string datos = cuerpo.substr(2);
        const bool categoria_valida = parametros::nombre_categoria(categoria) != nullptr;
        if (crc16(cuerpo) == crc) {
            const Tipo tipo =
                categoria_valida && utf8::valido(datos) ? Tipo::valido : Tipo::desconocido;
            lecturas.push_back({tipo, categoria, datos});
            esperado = inicio = fin;
            continue;
        }
        if (en_secuencia && categoria_valida) lecturas.push_back({Tipo::danado, categoria, datos});
        ++inicio;
    }
    return lecturas;
}

}  // namespace trama_registros
