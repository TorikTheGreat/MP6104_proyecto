"""
Vectores de prueba para el pipeline de punta a punta, para comparar con la 
implementación en C++. Se compara el canal codificado que queda en el WAV y 
lo que imprime al decodificarlo.

La entrada no se guarda: es ruido de un generador congruencial que C++ repite con
la misma fórmula (ruido()). Retardos cortos (100 y 150 us, ventanas de 175
muestras) para que el vector sea chico.
"""

import os
import tempfile

import numpy as np

import aplicacion as ap
import bloques_reales
from vectores import escribir

SEMILLA = 2026
VENTANAS = 80
ENMASCARAMIENTO = ["--retardo0", "100", "--retardo1", "150"]
METADATOS = ["--titulo", "Hola"]


def ruido(n: int, semilla: int = SEMILLA) -> list[int]:
    """Muestras entre -10000 y 10000: x <- (1103515245 x + 12345) mod 2^31."""
    x = semilla
    muestras = []
    for _ in range(n):
        x = (1103515245 * x + 12345) % 2**31
        muestras.append((x >> 8) % 20001 - 10000)
    return muestras


def generar(directorio):
    n = VENTANAS * 175
    muestras = ruido(2 * n)
    audio = ap.Audio(np.array(muestras[:n], dtype=np.int16), np.array(muestras[n:], dtype=np.int16))
    reales = bloques_reales.bloques_reales()

    class Salida:
        def __init__(self):
            self.texto = ""

        def write(self, s):
            self.texto += s

    with tempfile.TemporaryDirectory() as d:
        original = os.path.join(d, "original.wav").encode()
        codificado = os.path.join(d, "codificado.wav").encode()
        reales.escribir(original, audio, audio.izquierdo.astype(np.float64), True)
        ap.ejecutar([original, codificado] + [a.encode() for a in METADATOS + ENMASCARAMIENTO],
                    Salida(), Salida(), reales)
        salida, errores = Salida(), Salida()
        ap.ejecutar([codificado] + [a.encode() for a in ENMASCARAMIENTO], salida, errores, reales)
        canal = reales.leer(codificado).izquierdo

    def esc(nombre, tipo, valores):
        escribir(directorio, f"aplicacion_{nombre}", tipo, valores, generador=__file__,
                 semilla=SEMILLA)

    esc("codificado", "entero", [int(v) for v in canal])
    esc("salida", "texto", salida.texto.splitlines())
    esc("errores", "texto", errores.texto.splitlines())
