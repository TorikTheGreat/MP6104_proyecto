"""Vectores de parametros.py: conversión de retardos y escala a int16."""

import numpy as np

import parametros as p
from vectores import escribir

SEMILLA = 12


def generar(directorio):
    # Retardos: bordes, medias muestras exactas (5000 µs = 220.5 muestras) y al azar.
    rng = np.random.default_rng(SEMILLA)
    t_us = [1, 11, 12, 33, 34, 1000, 1500, 5000, 15000, 999_999, 1_000_000]
    t_us += [int(t) for t in rng.integers(1, 1_000_001, 50)]
    escribir(directorio, "parametros_retardos_us", "entero", t_us,
             generador=__file__, semilla=SEMILLA)
    escribir(directorio, "parametros_retardos_muestras", "entero",
             [p.muestras_desde_us(t) for t in t_us], generador=__file__, semilla=SEMILLA)

    # Escala: ±1, ±1.5 (saturación), medias unidades exactas y al azar.
    y = [0.0, 1.0, -1.0, 1.5, -1.5, 0.5 / 32768, -0.5 / 32768, 1.5 / 32768,
         -1.5 / 32768, 32766.5 / 32768, -32768.5 / 32768]
    y += list(rng.uniform(-1.2, 1.2, 200))
    escribir(directorio, "parametros_flotantes", "real", y, generador=__file__, semilla=SEMILLA)
    escribir(directorio, "parametros_int16", "entero", [p.a_int16(v) for v in y],
             generador=__file__, semilla=SEMILLA)
