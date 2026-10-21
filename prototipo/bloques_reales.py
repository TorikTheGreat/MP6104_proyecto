"""
Funciones para trabajar los bloques de audio en python, con la interfaz de aplicacion.py

- Leer y escribir el WAV (A): PCM de 16 bits, estéreo, 44.1 kHz; el WAV de salida
  tiene la misma cabecera que el original y el canal que no se codifica, sin cambios.
- Codificar (A): el algoritmo de codificador.py como función. Cada ventana se
  convoluciona con h_b(n) = delta(n) + a_b delta(n - t_b) y el resultado, de largo
  M + N - 1, se suma a partir del inicio de la ventana (solapamiento y suma). El
  audio después de la última ventana con bit se copia sin cambios.
- Decodificar (B): decodificador.decodificar_senal.
"""

import os
import wave

import numpy as np

import decodificador as dec
import parametros as p
from aplicacion import Audio, Bloques, Decodificacion, Eco, Ventana


def leer(ruta: bytes) -> Audio:
    try:
        with wave.open(os.fsdecode(ruta), "rb") as w:
            if (w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getcomptype()) != \
                    (2, 2, p.FS, "NONE"):
                raise RuntimeError("Se requiere WAV PCM 16 bits, estereo, 44100 Hz")
            crudo = w.readframes(w.getnframes())
    except (OSError, EOFError, wave.Error) as e:
        raise RuntimeError(f"No se pudo abrir: {os.fsdecode(ruta)}") from e
    estereo = np.frombuffer(crudo, dtype="<i2").reshape(-1, 2)
    return Audio(estereo[:, 0].copy(), estereo[:, 1].copy())


def escribir(ruta: bytes, original: Audio, codificado: np.ndarray, izquierdo: bool) -> int:
    """Devuelve cuántas muestras se saturaron al pasar a 16 bits."""
    recortes = int(np.count_nonzero((codificado > 32767) | (codificado < -32768)))
    nuevo = np.clip(codificado, -32768, 32767).astype(np.int16)
    otro = np.zeros(len(codificado), dtype=np.int16)
    sin_cambios = original.derecho if izquierdo else original.izquierdo
    otro[:len(sin_cambios)] = sin_cambios[:len(codificado)]
    estereo = np.stack([nuevo, otro] if izquierdo else [otro, nuevo], axis=1)
    with wave.open(os.fsdecode(ruta), "wb") as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(p.FS)
        w.writeframes(estereo.astype("<i2").tobytes())
    return recortes


def codificar_eco(x: np.ndarray, bits: list[int], d0: int, d1: int, a0: float, a1: float,
                  largo: int) -> np.ndarray:
    """Un bit por ventana para las primeras len(bits) ventanas; la salida tiene
    max(d0, d1) muestras más que x, por la cola de la última convolución."""
    x = x.astype(np.float64)
    h0 = np.zeros(d0 + 1)
    h0[0], h0[d0] = 1.0, a0
    h1 = np.zeros(d1 + 1)
    h1[0], h1[d1] = 1.0, a1
    salida = np.zeros(len(x) + max(d0, d1))
    for k, bit in enumerate(bits):
        inicio = k * largo
        ventana_codificada = np.convolve(x[inicio:inicio + largo], h1 if bit else h0, mode="full")
        salida[inicio:inicio + len(ventana_codificada)] += ventana_codificada
    fin = len(bits) * largo
    salida[fin:len(x)] += x[fin:]
    return salida


def codificar(canal: np.ndarray, bits: list[int], eco: Eco) -> np.ndarray:
    # Las ventanas sin bit del final quedan sin eco; las del medio son silencio
    # digital, y el eco de un silencio es silencio.
    usadas = len(bits)
    while usadas > 0 and bits[usadas - 1] < 0:
        usadas -= 1
    if usadas == 0:
        return canal.astype(np.float64)
    return codificar_eco(canal, [1 if b == 1 else 0 for b in bits[:usadas]],
                         eco.d0, eco.d1, eco.a0, eco.a1, eco.largo_ventana)


def decodificar(canal: np.ndarray, eco: Eco) -> Decodificacion:
    r = dec.decodificar_senal(canal, eco.d0, eco.d1, largo_ventana=eco.largo_ventana)
    return Decodificacion(r.bits, [Ventana(v.indice, v.bit, v.r0, v.r1, v.rms)
                                   for v in r.ventanas])


def bloques_reales() -> Bloques:
    return Bloques(leer, escribir, codificar, decodificar)
