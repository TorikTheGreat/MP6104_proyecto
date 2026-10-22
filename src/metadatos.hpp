/*

Equivalente a  prototipo/metadatos.py y ampliamente documentado para evitar
futura confusión.

Metadatos <-> bits, independiente del formato de la trama (equivale a
prototipo/metadatos.py).

Codificar: el formato produce una copia del mensaje; aquí se repite hasta llenar
las ventanas disponibles o se trunca si no cabe.
Decodificar: el formato lee los registros; aquí se decide qué se reporta.

Las decisiones de parametros.hpp (SI_NO_CABE, REPETIR_MENSAJE,
AVISOS_REQUIEREN_VALIDO, VOTAR_COPIAS) entran como argumentos con ese valor por defecto, para
poder probar las dos opciones (en Python las pruebas las cambian con mock).
tests/pruebas_metadatos.cpp compara con los vectores de prototipo/vectores_metadatos.py.
*/

#ifndef METADATOS_HPP
#define METADATOS_HPP

#include <cstddef>
#include <stdexcept>
#include <vector>

#include "parametros.hpp"
#include "trama.hpp"

namespace metadatos {

// El mensaje no cabe y SI_NO_CABE es error.
class NoCabe : public std::runtime_error {
public:
    NoCabe(std::size_t necesarios, std::size_t disponibles);
    std::size_t necesarios;
    std::size_t disponibles;
};

struct Codificacion {
    trama::Bits bits;        // un bit por ventana, en orden; puede ser más corto que las ventanas
    std::size_t necesarios;  // bits de una copia completa del mensaje
    bool truncado;           // true si no cupo una copia completa
};

// Lanza std::invalid_argument como trama_registros::codificar.
std::size_t bits_necesarios(const trama::Metadatos& metadatos);

// Repite bits cíclicamente hasta tener exactamente n. Lanza std::invalid_argument
// si n > 0 y no hay bits.
trama::Bits repetir(const trama::Bits& bits, std::size_t n);

// Bits para las n_ventanas que pueden llevar uno.
//
// Con repetir_mensaje el mensaje se repite hasta llenarlas; si no, va una vez y
// las ventanas sobrantes quedan sin eco. Si no cabe ni una copia: con
// SiNoCabe::truncar se codifica lo que cabe (quien llama avisa con
// notificacion::aviso_no_cabe); con SiNoCabe::error se lanza NoCabe. Con
// n_ventanas = 0 (audio sin ninguna ventana) el resultado es vacío y truncado.
// Lanza std::invalid_argument como trama_registros::codificar.
Codificacion a_bits(const trama::Metadatos& metadatos, std::size_t n_ventanas,
                    parametros::SiNoCabe si_no_cabe = parametros::SI_NO_CABE,
                    bool repetir_mensaje = parametros::REPETIR_MENSAJE);

// Registros válidos de la copia votada (votacion.hpp), cada uno una vez. La copia
// se lee dos veces seguidas porque es cíclica: un registro puede empezar al final
// y seguir al principio. Solo los válidos: los dañados de la copia votada no
// agregan nada y pueden ser de categorías que no se codificaron. Vacío si no se
// pudo votar.
std::vector<trama::Lectura> leer_votada(const trama::Bits& bits);

// Metadatos recuperados, sin duplicados, en orden de primera aparición.
//
// bits: los de las ventanas clasificadas como 0 o 1. Devuelve:
// - por cada categoría válida, el texto que aparece en más copias (empate: el
//   primero);
// - cada desconocido o dañado distinto, una vez. Un dañado no se reporta si su
//   categoría tiene una copia válida (es una copia dañada de algo ya reportado).
// Con avisos_requieren_valido, desconocidos y dañados solo se reportan si hubo
// algún registro válido: un archivo sin codificar no produce avisos.
// Con votar_copias, los registros de la copia votada se suman como una copia más.
// Las ventanas sin eco no se pasan: un bit que no es 0 o 1 lanza
// std::invalid_argument.
std::vector<trama::Lectura> desde_bits(
    const trama::Bits& bits,
    bool avisos_requieren_valido = parametros::AVISOS_REQUIEREN_VALIDO,
    bool votar_copias = parametros::VOTAR_COPIAS);

}  // namespace metadatos

#endif
