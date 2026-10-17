"""
Vectores de la FFT de C++ (src/fft.cpp): numpy.fft.fft e ifft de señales
complejas aleatorias de varios tamaños.

Las salidas de este script servirán para verificar la equivalencia entre
nuestra implementación en C++ de la FFT con respecto a la implementación en
Python del prototipo (que lo resuelve con una biblioteca).
"""

import numpy as np

from vectores import escribir

SEMILLA = 44100
TAMANOS = (1, 2, 8, 1024, 4096)


def generar(directorio):
    rng = np.random.default_rng(SEMILLA)
    for n in TAMANOS:
        x = rng.standard_normal(n) + 1j * rng.standard_normal(n)
        datos = {"generador": __file__, "semilla": SEMILLA}
        escribir(directorio, f"fft_{n}_entrada", "complejo", x, **datos)
        escribir(directorio, f"fft_{n}_fft", "complejo", np.fft.fft(x), **datos)
        escribir(directorio, f"fft_{n}_ifft", "complejo", np.fft.ifft(x), **datos)
