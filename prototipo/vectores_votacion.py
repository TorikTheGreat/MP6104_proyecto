"""
Vectores de prueba para la votación entre copias (votacion.py), con el propósito
de hacer la comparación con las contrapartes implementadas en C++ del algoritmo. 
Compone un mensaje de prueba y lo transforma de cinco formas distintas para simular
errores.
"""

import random

import metadatos as m
import votacion as v
from canal_simulado import canal, generador
from vectores import escribir
from vectores_metadatos import SIETE

SEMILLA = 7


def _casos():
    copia = len(m.formato().codificar(SIETE))
    mensaje = m.a_bits(SIETE, 5 * copia + 77).bits
    rng = random.Random(SEMILLA)
    # Pocos borrados o inserciones puntuales (como las ventanas en silencio que
    # descarta el decodificador), para que la alineación tenga que corregir.
    borrado = canal(mensaje, generador(SEMILLA + 1), p_inversion=0.03)
    del borrado[3 * copia + 500]
    del borrado[copia + 10]
    insercion = canal(mensaje, generador(SEMILLA + 2), p_inversion=0.03)
    insercion.insert(2 * copia + 700, 1)
    insercion.insert(copia // 2, 0)
    return [
        ("inversion_5", canal(mensaje, generador(SEMILLA), p_inversion=0.05)),
        ("borrado", borrado),
        ("insercion", insercion),
        ("cortada", canal(mensaje[333:], generador(SEMILLA + 3), p_inversion=0.03)),
        ("aleatorio", [rng.randint(0, 1) for _ in range(3000)]),
    ]


def generar(directorio):
    def esc(nombre, valores):
        escribir(directorio, f"votacion_{nombre}", "entero", valores,
                 generador=__file__, semilla=SEMILLA)

    for nombre, bits in _casos():
        periodo = v.estimar_periodo(bits)
        votada = v.votar_copias(bits)
        esc(f"{nombre}_entrada", bits)
        esc(f"{nombre}_periodo", [periodo or 0])
        esc(f"{nombre}_votada", votada or [])
