"""
Se propone repetir el mensaje a lo largo del audio: esto nos permite tener un 
mecanismo de corrección (votación entre copias del mensaje), a cambio de una
pérdida en la calidad del audio final. Estableceremos experimentalmente si ese
intercambio vale la pena, pero independientemente se parametrizará el flujo para
que sea una opción elegible.

La votación funcionará de esta forma: como la trama repite el mensaje, 
la secuencia de bits será periódica. Se estima entonces el período, se alinea 
cada copia con la anterior (tolerando unos pocos bits corridos) y cada bit se 
decide por mayoría entre las copias. La copia votada se lee como una copia más 
en metadatos.desde_bits.

Ahora: el período se busca por bloques, permitiendo un corrimiento de hasta DERIVA_MAX
bits; como eso hace que P-4 puntúe casi igual que P, después se elige el P
exacto entre los vecinos.

Nota: el decodificador rechaza un audio donde se detecte eco en menos de un
25% de sus ventanas así que al deshabilitar la repetición, se debe cumplir esa regla.
"""

from collections.abc import Sequence

import numpy as np

from trama import Bits

PERIODO_MIN = 48         # el registro más corto: 40 bits de sobrecarga y 1 byte
PERIODO_MAX = 7 * (40 + 8 * 255)  # el mensaje más largo: 7 registros de 255 bytes
MIN_COPIAS = 3
DERIVA_MAX = 4
BLOQUE = 64
UMBRAL_MILESIMAS = 750   # coincidencia mínima en el período
MARGEN_MILESIMAS = 20    # se prefiere el menor período a esta distancia del máximo
PASADAS = 2              # alinear y votar, dos veces


def estimar_periodo(bits: Sequence[int]) -> int | None:
    """Período del mensaje, o None si no hay al menos MIN_COPIAS copias claras."""
    b = np.asarray(bits, dtype=np.int8)
    n = b.size
    puntajes = []  # (P, coincidencias, comparaciones)
    for periodo in range(PERIODO_MIN, min(n // MIN_COPIAS, PERIODO_MAX) + 1):
        bloques = (n - periodo - DERIVA_MAX) // BLOQUE
        if bloques == 0:
            break
        largo = bloques * BLOQUE
        por_bloque = np.zeros(bloques, dtype=np.int64)
        for d in range(-DERIVA_MAX, DERIVA_MAX + 1):
            iguales = b[:largo] == b[periodo + d:periodo + d + largo]
            por_bloque = np.maximum(por_bloque, iguales.reshape(bloques, BLOQUE).sum(axis=1))
        puntajes.append((periodo, int(por_bloque.sum()), largo))
    if not puntajes:
        return None
    _, c_max, t_max = puntajes[0]
    for _, c, t in puntajes[1:]:
        if c * t_max > c_max * t:
            c_max, t_max = c, t
    if 1000 * c_max < UMBRAL_MILESIMAS * t_max:
        return None
    candidato = next(periodo for periodo, c, t in puntajes  # c/t >= c_max/t_max - margen
                     if 1000 * c * t_max >= 1000 * c_max * t - MARGEN_MILESIMAS * t * t_max)
    mejor, c_mejor, t_mejor = candidato, -1, 1
    for periodo in range(candidato, min(candidato + 2 * DERIVA_MAX, n // MIN_COPIAS) + 1):
        c = int(np.count_nonzero(b[:-periodo] == b[periodo:]))
        t = n - periodo
        if c * t_mejor > c_mejor * t:  # en un empate, el menor
            mejor, c_mejor, t_mejor = periodo, c, t
    return mejor


def _votar(bits: Sequence[int], inicios: Sequence[int], periodo: int) -> Bits:
    n = len(bits)
    votada: Bits = []
    for i in range(periodo):
        valores = [bits[s + i] for s in inicios if s + i < n]
        unos = sum(valores)
        ceros = len(valores) - unos
        if len(valores) >= MIN_COPIAS and unos != ceros:
            votada.append(1 if unos > ceros else 0)
        else:
            votada.append(valores[0])
    return votada


def _alinear(bits: Sequence[int], referencia: Sequence[int]) -> list[int]:
    """Inicio de cada copia, buscando cada una cerca del fin de la anterior."""
    n = len(bits)
    periodo = len(referencia)
    desfases = [0]
    for d in range(1, DERIVA_MAX + 1):
        desfases += [-d, d]
    inicios = [0]
    while inicios[-1] + periodo < n:
        esperado = inicios[-1] + periodo
        mejor, c_mejor, t_mejor = esperado, -1, 1
        for d in desfases:
            s = esperado + d
            if s <= inicios[-1] or s >= n:
                continue
            t = min(periodo, n - s)
            c = sum(1 for i in range(t) if bits[s + i] == referencia[i])
            if c * t_mejor > c_mejor * t:  # en un empate queda el desfase anterior
                mejor, c_mejor, t_mejor = s, c, t
        inicios.append(mejor)
    return inicios


def votar_copias(bits: Sequence[int]) -> Bits | None:
    """Una copia del mensaje votada bit a bit, o None si no se pudo estimar el
    período. La copia empieza donde empieza bits, no necesariamente en un registro."""
    bits = list(bits)
    periodo = estimar_periodo(bits)
    if periodo is None:
        return None
    votada = _votar(bits, range(0, len(bits), periodo), periodo)
    for _ in range(PASADAS):
        votada = _votar(bits, _alinear(bits, votada), periodo)
    return votada
