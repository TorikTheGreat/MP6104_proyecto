/*
El programa ecoesteg sin los bloques de audio (equivale a prototipo/aplicacion.py):
lee los argumentos, decide el modo (R28, R39) y une la trama, la votación y la
notificación con los bloques de A (leer y escribir el WAV, codificar) y de B
(decodificar), que entran como funciones. main.cpp le pasa los reales (bloques_reales.cpp); las pruebas, unos
falsos, para probar el flujo sin audio.

Códigos de salida: 0 éxito (también si el mensaje no cupo entero o si no se
encontraron metadatos), 1 error de ejecución (archivo, formato WAV), 2 error de uso.
*/

#ifndef APLICACION_HPP
#define APLICACION_HPP

#include <cstddef>
#include <cstdint>
#include <functional>
#include <ostream>
#include <string>
#include <vector>

#include "trama.hpp"

namespace aplicacion {

struct Audio {
    std::vector<std::int16_t> izquierdo;
    std::vector<std::int16_t> derecho;
};

// Parámetros de enmascaramiento ya convertidos: retardos y ventana en muestras,
// amplitudes del eco en [0, 1).
struct Eco {
    std::size_t d0;
    std::size_t d1;
    std::size_t largo_ventana;
    double a0;
    double a1;
};

struct Ventana {
    std::size_t indice;
    int bit;  // 0, 1 o -1 (sin eco)
    double r0;
    double r1;
    double rms;
};

struct Decodificacion {
    std::vector<int> bits;          // los que pasan a la trama, en orden
    std::vector<Ventana> ventanas;  // todas, para --diagnostico
};

struct Bloques {
    // Lee un WAV PCM de 16 bits, estéreo, 44.1 kHz (R25); lanza std::exception.
    std::function<Audio(const std::string& ruta)> leer;
    // Escribe el canal codificado y copia el otro del original (R26, R37).
    // Devuelve cuántas muestras se saturaron al pasar a 16 bits.
    std::function<std::size_t(const std::string& ruta, const Audio& original,
                              const std::vector<double>& codificado, bool izquierdo)>
        escribir;
    // Un bit por ventana, en orden (0, 1, o -1 para dejarla sin eco), con
    // convolución y solapamiento y suma (R34, R36).
    std::function<std::vector<double>(const std::vector<std::int16_t>& canal,
                                      const std::vector<int>& bits, const Eco& eco)>
        codificar;
    // Autocorrelación del cepstro y clasificador (R42-R47).
    std::function<Decodificacion(const std::vector<std::int16_t>& canal, const Eco& eco)>
        decodificar;
};

// args: los argumentos sin el nombre del programa. Escribe los metadatos en
// salida y los avisos y errores en error; devuelve el código de salida.
int ejecutar(const std::vector<std::string>& args, std::ostream& salida, std::ostream& error,
             const Bloques& bloques);

// Qué ventanas llevan bit al codificar: las que no son silencio digital en el audio
// codificado, que son las que el decodificador lee. El eco de una ventana se
// derrama sobre la siguiente, así que se codifica y se vuelve a mirar hasta que no
// cambie (casi siempre basta una vez).
std::vector<bool> ventanas_con_bit(const std::vector<std::int16_t>& canal,
                                   const trama::Metadatos& metadatos, const Eco& eco,
                                   const Bloques& bloques);

// Reparte los bits del mensaje en las ventanas que llevan bit; las demás van con -1.
std::vector<int> bits_por_ventana(const std::vector<int>& mensaje,
                                  const std::vector<bool>& lleva_bit);

}  // namespace aplicacion

#endif
