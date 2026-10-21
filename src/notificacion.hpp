/*
Notificación de metadatos (R48-R51) y avisos del codificador (equivale a prototipo/notificacion.py).

Recibe los metadatos ya interpretados de metadatos::desde_bits y
arma dos listas de líneas: los metadatos válidos, para la salida estándar, junto con su
categoría; y los desconocidos o dañados, para la salida de error, con una
advertencia. tests/pruebas_notificacion.cpp compara con los vectores de
prototipo/vectores_notificacion.py.
*/

#ifndef NOTIFICACION_HPP
#define NOTIFICACION_HPP

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "trama.hpp"

namespace notificacion {

// Cada byte que no forma parte de una secuencia UTF-8 válida, y cada carácter de
// control, se cambia por "?": un registro dañado no puede partir la línea ni
// cambiar la terminal.
std::string texto_visible(const std::string& datos);

// Líneas para la salida estándar (first) y para la de error (second). Lanza
// std::invalid_argument si una lectura válida tiene una categoría que no es 1-7
// (metadatos::desde_bits nunca la produce).
std::pair<std::vector<std::string>, std::vector<std::string>> notificar(
    const std::vector<trama::Lectura>& resultados);

// Aviso del codificador cuando el mensaje no cabe y se trunca (R31): solo con
// Codificacion::truncado, es decir, disponibles < necesarios. Con valores reales
// (miles de bits, ventanas de miles de muestras) da lo mismo que Python.
std::vector<std::string> aviso_no_cabe(std::int64_t necesarios, std::int64_t disponibles,
                                       std::int64_t largo_ventana);

}  // namespace notificacion

#endif
