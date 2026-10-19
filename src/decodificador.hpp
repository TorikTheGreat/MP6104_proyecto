// Dueño: B.
/*
Decodificador de esteganografía por enmascaramiento con eco (R42-R47).

Evalúa la autocorrelación del cepstro en el dominio espectral (R44, R45) usando
la FFT propia de src/fft.hpp:
    R_cc(n) = |IFFT( (ln max(|FFT(v_norm)|, EPSILON_ESPECTRO))^2 )|
sobre cada ventana normalizada por su valor eficaz (RMS) para garantizar
invarianza ante la escala de la señal, y clasifica cada ventana como 0, 1 o
SIN_ECO (-1) según la magnitud en los retardos d0 y d1 (R43, R47).

Usa los mismos nombres de funciones y constantes que prototipo/decodificador.py.
*/

#ifndef DECODIFICADOR_HPP
#define DECODIFICADOR_HPP

#include <cstddef>
#include <cstdint>
#include <vector>

#include "parametros.hpp"

constexpr int SIN_ECO = -1;
constexpr double RMS_REFERENCIA = 1000.0;
constexpr double UMBRAL_SILENCIO = 1e-5;
constexpr double UMBRAL_ECO = 0.70;
constexpr double EPSILON_ESPECTRO = 1e-12;
constexpr double FRACCION_MIN_VENTANAS_CON_ECO = 0.25;

struct DiagnosticoVentana {
    std::size_t indice = 0;
    int bit = SIN_ECO;
    double r0 = 0.0;
    double r1 = 0.0;
    double rms = 0.0;
};

struct ResultadoDecodificacion {
    std::vector<int> bits;
    std::vector<DiagnosticoVentana> ventanas;
};

// Valor eficaz (RMS) de una ventana.
double rms_ventana(const std::vector<double>& ventana);
double rms_ventana(const std::vector<std::int16_t>& ventana);

// Escala la ventana para que tenga RMS = RMS_REFERENCIA, o ceros si es silenciosa.
std::vector<double> normalizar_ventana(const std::vector<double>& ventana);
std::vector<double> normalizar_ventana(const std::vector<std::int16_t>& ventana);

// Magnitud de la autocorrelación del cepstro evaluada en el dominio espectral (R44, R45).
std::vector<double> autocorrelacion_cepstro(const std::vector<double>& ventana);
std::vector<double> autocorrelacion_cepstro(const std::vector<std::int16_t>& ventana);

// Evalúa una ventana y devuelve su clasificación (0, 1 o SIN_ECO) y métricas.
DiagnosticoVentana evaluar_ventana(const std::vector<double>& ventana, std::size_t d0,
                                   std::size_t d1, double umbral_eco = UMBRAL_ECO,
                                   std::size_t indice = 0);
DiagnosticoVentana evaluar_ventana(const std::vector<std::int16_t>& ventana, std::size_t d0,
                                   std::size_t d1, double umbral_eco = UMBRAL_ECO,
                                   std::size_t indice = 0);

// Clasifica una ventana como 0, 1 o SIN_ECO (-1) (R43, R47).
int clasificar_ventana(const std::vector<double>& ventana, std::size_t d0, std::size_t d1,
                       double umbral_eco = UMBRAL_ECO);
int clasificar_ventana(const std::vector<std::int16_t>& ventana, std::size_t d0, std::size_t d1,
                       double umbral_eco = UMBRAL_ECO);

// Segmenta la señal en ventanas y decodifica la secuencia de bits y diagnósticos (R41-R47).
ResultadoDecodificacion decodificar_senal(const std::vector<double>& senal, std::size_t d0,
                                          std::size_t d1, std::size_t largo_ventana = 0,
                                          double umbral_eco = UMBRAL_ECO);
ResultadoDecodificacion decodificar_senal(const std::vector<std::int16_t>& senal, std::size_t d0,
                                          std::size_t d1, std::size_t largo_ventana = 0,
                                          double umbral_eco = UMBRAL_ECO);

// Conveniencia: convierte retardos de microsegundos a muestras y decodifica.
ResultadoDecodificacion decodificar_por_us(
    const std::vector<double>& senal, std::int64_t retardo0_us = parametros::RETARDO0_US,
    std::int64_t retardo1_us = parametros::RETARDO1_US, double umbral_eco = UMBRAL_ECO);
ResultadoDecodificacion decodificar_por_us(
    const std::vector<std::int16_t>& senal, std::int64_t retardo0_us = parametros::RETARDO0_US,
    std::int64_t retardo1_us = parametros::RETARDO1_US, double umbral_eco = UMBRAL_ECO);

#endif
