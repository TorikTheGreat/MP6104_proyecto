"""
Una pequeña simulación del canal para probar la trama. Una de nuestros requerimientos
es que tengamos una tasa de recuperación de por lo menos 90% por bit. Es decir, tenemos
que poder tolerar errores en por lo menos 1 de cada 10 bits. Este script nos permitirá
generar datos por medio de tres pruebas:

- inversión: el decodificador toma un 0 por 1 o al revés;
- borrado: una ventana con bit se toma como «sin eco», y su bit desaparece;
- inserción: una ventana sin bit se toma como 0 o 1, y aparece un bit de más.
"""

from collections.abc import Iterable

import numpy as np


def canal(bits: Iterable[int], rng: np.random.Generator, p_inversion: float = 0.0,
          p_borrado: float = 0.0, p_insercion: float = 0.0) -> list[int]:
    """bits pasados por el canal; rng es un np.random.Generator."""
    salida: list[int] = []
    for b in bits:
        if rng.random() < p_insercion:
            salida.append(int(rng.integers(2)))
        if rng.random() < p_borrado:
            continue
        salida.append(b ^ 1 if rng.random() < p_inversion else b)
    return salida


def generador(semilla: int) -> np.random.Generator:
    return np.random.default_rng(semilla)
