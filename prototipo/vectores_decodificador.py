# Dueño: B.
"""Vectores de referencia de decodificador.py para comparar contra src/decodificador.cpp."""

import numpy as np

import decodificador as dec
import parametros as p
from vectores import escribir

SEMILLA = 44100


def generar(directorio):
    rng = np.random.default_rng(SEMILLA)
    datos = {"generador": __file__, "semilla": SEMILLA}

    d0 = p.muestras_desde_us(p.RETARDO0_US)
    d1 = p.muestras_desde_us(p.RETARDO1_US)
    largo = p.largo_ventana(p.RETARDO0_US, p.RETARDO1_US)

    # 1. Ventanas individuales con eco en d0 (bit 0) y en d1 (bit 1).
    base0 = rng.standard_normal(largo) * 4000.0
    v0 = base0.copy()
    v0[d0:] += 0.3 * base0[:-d0]
    rcc0 = dec.autocorrelacion_cepstro(v0)
    escribir(directorio, "decodificador_ventana_0_entrada", "real", v0, **datos)
    escribir(directorio, "decodificador_ventana_0_rcc", "real", rcc0, **datos)

    base1 = rng.standard_normal(largo) * 0.05  # señal débil para verificar normalización
    v1 = base1.copy()
    v1[d1:] += 0.3 * base1[:-d1]
    rcc1 = dec.autocorrelacion_cepstro(v1)
    escribir(directorio, "decodificador_ventana_1_entrada", "real", v1, **datos)
    escribir(directorio, "decodificador_ventana_1_rcc", "real", rcc1, **datos)

    # 2. Señal multiventana en int16 con 8 ventanas codificadas y 4 silenciosas al final.
    bits_ref = [1, 0, 1, 1, 0, 0, 1, 0]
    n_total = len(bits_ref) + 4
    x = rng.standard_normal(n_total * largo) * 5000.0
    x[len(bits_ref) * largo :] = 0.0
    y = x.copy()
    for k, b in enumerate(bits_ref):
        d = d0 if b == 0 else d1
        ini = k * largo
        fin = ini + largo
        dest_fin = min(len(bits_ref) * largo, fin + d)
        n_eco = dest_fin - (ini + d)
        y[ini + d : dest_fin] += 0.3 * x[ini : ini + n_eco]
    y_int16 = np.clip(y, -32768, 32767).astype(np.int16)

    res = dec.decodificar_senal(y_int16, d0, d1, largo_ventana=largo)
    escribir(directorio, "decodificador_senal_entrada", "entero", y_int16, **datos)
    escribir(directorio, "decodificador_senal_bits_esperados", "entero", res.bits, **datos)
    escribir(
        directorio,
        "decodificador_senal_bits_ventanas",
        "entero",
        [v.bit for v in res.ventanas],
        **datos,
    )
    escribir(
        directorio,
        "decodificador_senal_r0",
        "real",
        [v.r0 for v in res.ventanas],
        **datos,
    )
    escribir(
        directorio,
        "decodificador_senal_r1",
        "real",
        [v.r1 for v in res.ventanas],
        **datos,
    )
