"""Vectores de ejemplo: prueban el viaje Python → archivo → C++ de punta a punta
(tests/pruebas_ejemplo.cpp) y sirven de plantilla para vectores_<módulo>.py."""

import numpy as np

from vectores import escribir

SEMILLA = 2026


def generar(directorio):
    rng = np.random.default_rng(SEMILLA)
    x = rng.standard_normal(16)
    datos = {"generador": __file__, "semilla": SEMILLA}

    # Entrada y salida: C++ calcula la suma acumulada y la exponencial compleja
    # de la entrada y compara con lo que calculó NumPy.
    escribir(directorio, "ejemplo_entrada", "real", x, **datos)
    escribir(directorio, "ejemplo_suma_acumulada", "real", np.cumsum(x), **datos)
    escribir(directorio, "ejemplo_exponencial", "complejo", np.exp(1j * x), **datos)

    # Valores fijos: C++ verifica que los lee exactamente.
    escribir(directorio, "ejemplo_enteros", "entero",
             [0, -1, 32767, -32768, 2**40], generador=__file__)
    escribir(directorio, "ejemplo_texto", "texto", [
        "Canción Ñandú",
        "",
        "salto\nde línea",
        "barra \\ invertida",
        "#almohadilla al inicio",
        "tab\tcontrol\x7f",
        b"byte inv\xe1lido",
    ], generador=__file__)
