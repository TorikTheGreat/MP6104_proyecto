"""
Este es un módulo que contiene funciones utilizadas por punta_a_punta.py (el script
que ejecuta el flujo entero). También son utilizadas por test_aplicacion.py para
hacer pruebas.

Es básicamente el programa ecoesteg sin los bloques de audio: lee los argumentos, 
decide el modo (R28, R39) y une la trama, la votación y la notificación con los bloques 
de A (leer y escribir el WAV, codificar) y de B (decodificar), que entran como
funciones. 

Códigos de salida: 0 éxito (también si el mensaje no cupo entero o si no se
encontraron metadatos), 1 error de ejecución (archivo, formato WAV), 2 error de uso.
"""

import os
import wave
from collections.abc import Callable, Sequence
from dataclasses import dataclass
from typing import TextIO

import numpy as np

import decodificador as dec
import linea_comandos as lc
import metadatos as m
import notificacion as n
import parametros as p
from trama import Metadatos


@dataclass
class Audio:
    izquierdo: np.ndarray  # int16
    derecho: np.ndarray


@dataclass
class Eco:
    """Parámetros de enmascaramiento ya convertidos: retardos y ventana en
    muestras, amplitudes del eco en [0, 1)."""
    d0: int
    d1: int
    largo_ventana: int
    a0: float
    a1: float


@dataclass
class Ventana:
    indice: int
    bit: int  # 0, 1 o -1 (sin eco)
    r0: float
    r1: float
    rms: float


@dataclass
class Decodificacion:
    bits: list[int]          # los que pasan a la trama, en orden
    ventanas: list[Ventana]  # todas, para --diagnostico


@dataclass
class Bloques:
    # Lee un WAV PCM de 16 bits, estéreo, 44.1 kHz (R25); lanza una excepción si no.
    leer: Callable[[bytes], Audio]
    # (ruta, original, codificado, izquierdo) -> muestras saturadas. Escribe el
    # canal codificado y copia el otro del original (R26, R37).
    escribir: Callable[[bytes, Audio, np.ndarray, bool], int]
    # (canal, un bit por ventana: 0, 1 o -1 sin eco, eco) -> canal codificado, con
    # convolución y solapamiento y suma (R34, R36).
    codificar: Callable[[np.ndarray, list[int], Eco], np.ndarray]
    # Autocorrelación del cepstro y clasificador (R42-R47).
    decodificar: Callable[[np.ndarray, Eco], Decodificacion]


def bits_por_ventana(mensaje: Sequence[int], lleva_bit: Sequence[bool]) -> list[int]:
    """Reparte los bits del mensaje en las ventanas que llevan bit; las demás, -1."""
    bits = [-1] * len(lleva_bit)
    j = 0
    for k, lleva in enumerate(lleva_bit):
        if j == len(mensaje):
            break
        if lleva:
            bits[k] = mensaje[j]
            j += 1
    return bits


def _con_sonido(canal: np.ndarray, n_ventanas: int, largo: int) -> list[bool]:
    return [bool(np.any(canal[k * largo:(k + 1) * largo])) for k in range(n_ventanas)]


def _a_16_bits(y: np.ndarray) -> np.ndarray:
    """La misma conversión que al escribir el WAV: saturar y truncar."""
    return np.clip(y, -32768, 32767).astype(np.int16)


def ventanas_con_bit(canal: np.ndarray, metadatos: Metadatos, eco: Eco,
                     bloques: Bloques) -> list[bool]:
    """Qué ventanas llevan bit al codificar: las que no son silencio digital en el
    audio codificado, que son las que el decodificador lee. El eco de una ventana
    se derrama sobre la siguiente, así que se codifica y se vuelve a mirar hasta
    que no cambie (casi siempre basta una vez)."""
    largo = eco.largo_ventana
    n_ventanas = len(canal) // largo
    lleva = _con_sonido(canal, n_ventanas, largo)
    if all(lleva):
        return lleva
    for _ in range(4):
        mensaje = m.a_bits(metadatos, sum(lleva)).bits
        if not mensaje:
            return lleva
        codificado = _a_16_bits(bloques.codificar(canal, bits_por_ventana(mensaje, lleva), eco))
        nuevo = _con_sonido(codificado, n_ventanas, largo)
        if nuevo == lleva:
            break
        lleva = nuevo
    return lleva


def _eco_desde(c: lc.Configuracion) -> Eco:
    r0, r1, a0, a1 = lc.resolver(c)
    return Eco(p.muestras_desde_us(r0), p.muestras_desde_us(r1), p.largo_ventana(r0, r1),
               p.amplitud_desde_porcentaje(a0), p.amplitud_desde_porcentaje(a1))


def _escribir_csv(ruta: bytes, lineas: list[str]) -> None:
    with open(ruta, "w", encoding="utf-8") as f:
        f.writelines(linea + "\n" for linea in lineas)


def _codificar(c: lc.Configuracion, audio: Audio, izquierdo: bool, eco: Eco,
               error: TextIO, bloques: Bloques) -> int:
    canal = audio.izquierdo if izquierdo else audio.derecho
    lleva_bit = ventanas_con_bit(canal, c.metadatos, eco, bloques)
    cod = m.a_bits(c.metadatos, sum(lleva_bit))
    bits = bits_por_ventana(cod.bits, lleva_bit)
    codificado = (bloques.codificar(canal, bits, eco) if cod.bits
                  else canal.astype(np.float64))
    recortes = bloques.escribir(c.salida, audio, codificado, izquierdo)
    if cod.truncado:
        for linea in n.aviso_no_cabe(cod.necesarios, len(cod.bits), eco.largo_ventana):
            print(linea, file=error)
    if recortes:
        print(f"ADVERTENCIA: {recortes} muestras se saturaron al escribir el audio", file=error)
    if c.diagnostico is not None:
        _escribir_csv(c.diagnostico, ["ventana,bit"] + [f"{k},{b}" for k, b in enumerate(bits)])
    return lc.EXITO


def _decodificar(c: lc.Configuracion, audio: Audio, izquierdo: bool, eco: Eco,
                 salida: TextIO, error: TextIO, bloques: Bloques) -> int:
    d = bloques.decodificar(audio.izquierdo if izquierdo else audio.derecho, eco)
    lineas, avisos = n.notificar(m.desde_bits(d.bits))
    for linea in lineas:
        print(linea, file=salida)
    for linea in avisos:
        print(linea, file=error)
    if c.diagnostico is not None:
        _escribir_csv(c.diagnostico, ["ventana,bit,r0,r1,rms"] + [
            f"{v.indice},{v.bit},{v.r0:.17g},{v.r1:.17g},{v.rms:.17g}" for v in d.ventanas])
    return lc.EXITO


def ejecutar(args: Sequence[bytes], salida: TextIO, error: TextIO, bloques: Bloques) -> int:
    """args: los argumentos sin el nombre del programa. Escribe los metadatos en
    salida y los avisos y errores en error; devuelve el código de salida."""
    c = lc.analizar(args)
    if isinstance(c, lc.ErrorUso):
        print(f"error: {c.mensaje}\n\n{lc.USO}", file=error, end="")
        return lc.ERROR_USO
    if c.modo == "ayuda":
        print(lc.USO, file=salida, end="")
        return lc.EXITO
    try:
        eco = _eco_desde(c)
        audio = bloques.leer(c.entrada)
        izquierdo = c.canal == "izquierdo"
        if c.modo == "codificar":
            return _codificar(c, audio, izquierdo, eco, error, bloques)
        return _decodificar(c, audio, izquierdo, eco, salida, error, bloques)
    except Exception as e:  # todo error de ejecución sale con código 1
        print(f"error: {e}", file=error)
        return lc.ERROR_EJECUCION


# ---- Bloques reales -----------------------------------------------------------
#
# Funciones para trabajar los bloques de audio en python, con la interfaz de aplicacion.py
#
# - Leer y escribir el WAV (A): PCM de 16 bits, estéreo, 44.1 kHz; el WAV de salida
#   tiene la misma cabecera que el original y el canal que no se codifica, sin cambios.
# - Codificar (A): el algoritmo de codificador.py como función. Cada ventana se
#   convoluciona con h_b(n) = delta(n) + a_b delta(n - t_b) y el resultado, de largo
#   M + N - 1, se suma a partir del inicio de la ventana (solapamiento y suma). El
#   audio después de la última ventana con bit se copia sin cambios.
# - Decodificar (B): decodificador.decodificar_senal.


def leer_wav(ruta: bytes) -> Audio:
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


def escribir_wav(ruta: bytes, original: Audio, codificado: np.ndarray, izquierdo: bool) -> int:
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


def codificar_canal(canal: np.ndarray, bits: list[int], eco: Eco) -> np.ndarray:
    # Las ventanas sin bit del final quedan sin eco; las del medio son silencio
    # digital, y el eco de un silencio es silencio.
    usadas = len(bits)
    while usadas > 0 and bits[usadas - 1] < 0:
        usadas -= 1
    if usadas == 0:
        return canal.astype(np.float64)
    return codificar_eco(canal, [1 if b == 1 else 0 for b in bits[:usadas]],
                         eco.d0, eco.d1, eco.a0, eco.a1, eco.largo_ventana)


def decodificar_canal(canal: np.ndarray, eco: Eco) -> Decodificacion:
    r = dec.decodificar_senal(canal, eco.d0, eco.d1, largo_ventana=eco.largo_ventana)
    return Decodificacion(r.bits, [Ventana(v.indice, v.bit, v.r0, v.r1, v.rms)
                                   for v in r.ventanas])


def bloques_reales() -> Bloques:
    return Bloques(leer_wav, escribir_wav, codificar_canal, decodificar_canal)
