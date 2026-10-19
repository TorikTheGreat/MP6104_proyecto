// Datos y ayudas comunes a las pruebas de la trama, los metadatos y la
// notificación. SIETE es el mismo de prototipo/vectores_metadatos.py (los vectores
// se generan con él).

#ifndef TRAMA_DE_PRUEBA_HPP
#define TRAMA_DE_PRUEBA_HPP

#include <initializer_list>
#include <string>

#include "parametros.hpp"
#include "trama.hpp"
#include "trama_registros.hpp"

inline const trama::Metadatos SIETE = {
    {parametros::TITULO, "Canción de prueba"},
    {parametros::ARTISTA, "Ñandú y los Pingüinos"},
    {parametros::AUTOR, "José Pérez"},
    {parametros::ALBUM, "Álbum único"},
    {parametros::ANIO, "2026"},
    {parametros::INVITADO, "Zoë"},
    {parametros::PRODUCTOR, "Estudio Ártico"},
};

// Bits de un registro, con el CRC dado o el correcto (crc < 0).
inline trama::Bits registro(int categoria, const std::string& datos, int crc = -1) {
    namespace f = trama_registros;
    std::string cuerpo;
    cuerpo += static_cast<char>(categoria);
    cuerpo += static_cast<char>(datos.size());
    cuerpo += datos;
    const int c = crc < 0 ? f::crc16(cuerpo) : crc;
    return f::bits_de_bytes(static_cast<char>(f::PREAMBULO) + cuerpo +
                            static_cast<char>(c >> 8) + static_cast<char>(c & 0xFF));
}

inline trama::Bits unir(std::initializer_list<trama::Bits> partes) {
    trama::Bits todo;
    for (const auto& p : partes) todo.insert(todo.end(), p.begin(), p.end());
    return todo;
}

#endif
