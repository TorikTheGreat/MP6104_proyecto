/*
Transformada rápida de Fourier (FFT) propia, sin bibliotecas (R1).

Consiste en una Cooley-Tukey iterativa de base 2, con permutación por inversión de bits y log2(N)
etapas de mariposas. Se calculan los factores de giro w_N^k = exp(-j*2*pi*k/N) una
vez por tamaño con std::polar y se guardan.

La tabla es estado estático: fft e ifft no se deben llamar desde varios hilos a
la vez (el programa usa uno solo).

Misma convención que numpy.fft: fft sin escala, ifft divide entre N
(tests/pruebas_fft.cpp compara con los vectores de prototipo/vectores_fft.py).

Nota: fft(x) modifica x directamente, no devuelve un vector nuevo.
*/

#ifndef FFT_HPP
#define FFT_HPP

#include <complex>
#include <cstddef>
#include <vector>

using complejo = std::complex<double>;

/* El tamaño debe ser potencia de 2 y >= 1; si no, tira
 std::invalid_argument. Para una ventana de largo L, hay que rellenar con ceros hasta
 potencia_de_2_siguiente(L).*/
void fft(std::vector<complejo>& x);

// Inversa, en el lugar; divide entre N, igual que numpy.fft.ifft.
void ifft(std::vector<complejo>& x);

// Menor potencia de 2 >= n; n = 0 da 1. tira std::overflow_error si no cabe.
std::size_t potencia_de_2_siguiente(std::size_t n);

#endif
