# Dueño: B.
"""Decodificador de esteganografía por enmascaramiento con eco (R42-R47).

Evalúa la autocorrelación del cepstro en el dominio espectral (R44, R45):
    R_cc(n) = |IFFT( (ln |FFT(v_norm(n))|)^2 )|
sobre cada ventana normalizada por su valor eficaz (RMS) para garantizar
invarianza ante la escala de la señal, y clasifica cada ventana como 0, 1
o SIN_ECO (-1) según la magnitud en los retardos d0 y d1 (R43, R47).

Usa los mismos nombres de funciones y constantes que src/decodificador.hpp.
"""

from collections.abc import Sequence
from dataclasses import dataclass
from typing import Any

import numpy as np
import numpy.typing as npt

import parametros as p

SIN_ECO = -1
RMS_REFERENCIA = 1000.0
UMBRAL_SILENCIO = 1e-5
UMBRAL_ECO = 0.70
EPSILON_ESPECTRO = 1e-12
FRACCION_MIN_VENTANAS_CON_ECO = 0.25


@dataclass
class DiagnosticoVentana:
    indice: int
    bit: int
    r0: float
    r1: float
    rms: float


@dataclass
class ResultadoDecodificacion:
    bits: list[int]
    ventanas: list[DiagnosticoVentana]


def potencia_de_2_siguiente(n: int) -> int:
    """Menor potencia de 2 mayor o igual a n (mínimo 1)."""
    if n < 0:
        raise ValueError(f"el tamaño no puede ser negativo: {n}")
    if n <= 1:
        return 1
    return 1 << (int(n) - 1).bit_length()


def rms_ventana(ventana: Sequence[float | int] | npt.NDArray[Any]) -> float:
    """Valor eficaz (RMS) de una ventana."""
    v = np.asarray(ventana, dtype=np.float64)
    if v.ndim != 1 or v.size == 0:
        raise ValueError("la ventana debe ser un vector 1D no vacío")
    if not np.isfinite(v).all():
        raise ValueError("la ventana contiene muestras NaN o infinitas")
    return float(np.sqrt(np.mean(v * v)))


def normalizar_ventana(
    ventana: Sequence[float | int] | npt.NDArray[Any],
) -> npt.NDArray[np.float64]:
    """Escala la ventana para que tenga RMS = RMS_REFERENCIA, o ceros si es silenciosa."""
    v = np.asarray(ventana, dtype=np.float64)
    rms = rms_ventana(v)
    if rms <= UMBRAL_SILENCIO:
        return np.zeros(v.size, dtype=np.float64)
    return v * (RMS_REFERENCIA / rms)


def autocorrelacion_cepstro(
    ventana: Sequence[float | int] | npt.NDArray[Any],
) -> npt.NDArray[np.float64]:
    """Magnitud de la autocorrelación del cepstro evaluada en el dominio espectral (R44, R45).

    Rellena con ceros hasta la siguiente potencia de 2 del largo de la ventana:
        R_cc(n) = |IFFT( (ln max(|FFT(v_norm)|, EPSILON_ESPECTRO))^2 )|
    """
    v_norm = normalizar_ventana(ventana)
    n_fft = potencia_de_2_siguiente(v_norm.size)
    if not np.any(v_norm):
        return np.zeros(n_fft, dtype=np.float64)
    espectro = np.fft.fft(v_norm, n=n_fft)
    magnitud = np.maximum(np.abs(espectro), EPSILON_ESPECTRO)
    log_espectro = np.log(magnitud)
    rcc = np.fft.ifft(log_espectro * log_espectro)
    return np.abs(rcc).astype(np.float64)


def evaluar_ventana(
    ventana: Sequence[float | int] | npt.NDArray[Any],
    d0: int,
    d1: int,
    umbral_eco: float = UMBRAL_ECO,
    indice: int = 0,
) -> DiagnosticoVentana:
    """Evalúa una ventana y devuelve su clasificación (0, 1 o SIN_ECO) y métricas."""
    v = np.asarray(ventana, dtype=np.float64)
    if v.ndim != 1 or v.size == 0:
        raise ValueError("la ventana debe ser un vector 1D no vacío")
    if d0 <= 0 or d1 <= 0 or d0 == d1:
        raise ValueError(f"retardos inválidos: d0={d0}, d1={d1}")
    if max(d0, d1) >= v.size:
        raise ValueError(
            f"los retardos ({d0}, {d1}) deben ser menores que el largo de la ventana ({v.size})"
        )
    if umbral_eco < 0.0 or not np.isfinite(umbral_eco):
        raise ValueError(f"umbral_eco inválido: {umbral_eco}")

    rms = rms_ventana(v)
    if rms <= UMBRAL_SILENCIO:
        return DiagnosticoVentana(indice=indice, bit=SIN_ECO, r0=0.0, r1=0.0, rms=rms)

    rcc = autocorrelacion_cepstro(v)
    r0 = float(rcc[d0])
    r1 = float(rcc[d1])
    pico = max(r0, r1)
    if pico < umbral_eco or r0 == r1:
        bit = SIN_ECO
    elif r0 > r1:
        bit = 0
    else:
        bit = 1
    return DiagnosticoVentana(indice=indice, bit=bit, r0=r0, r1=r1, rms=rms)


def clasificar_ventana(
    ventana: Sequence[float | int] | npt.NDArray[Any],
    d0: int,
    d1: int,
    umbral_eco: float = UMBRAL_ECO,
) -> int:
    """Clasifica una ventana como 0, 1 o SIN_ECO (-1) (R43, R47)."""
    return evaluar_ventana(ventana, d0, d1, umbral_eco=umbral_eco).bit


def decodificar_senal(
    senal: Sequence[float | int] | npt.NDArray[Any],
    d0: int,
    d1: int,
    largo_ventana: int = 0,
    umbral_eco: float = UMBRAL_ECO,
) -> ResultadoDecodificacion:
    """Segmenta la señal en ventanas y decodifica la secuencia de bits y diagnósticos (R41-R47).

    Si largo_ventana == 0, usa K_VENTANA * max(d0, d1).
    Reconoce señales o colas sin codificar cuando las ventanas carecen de eco (R47).
    """
    x = np.asarray(senal, dtype=np.float64)
    if x.ndim != 1:
        raise ValueError("la señal del canal debe ser un vector 1D")
    if d0 <= 0 or d1 <= 0 or d0 == d1:
        raise ValueError(f"retardos inválidos: d0={d0}, d1={d1}")
    largo = int(p.K_VENTANA * max(d0, d1)) if largo_ventana == 0 else int(largo_ventana)
    if largo <= max(d0, d1):
        raise ValueError(
            f"el largo de ventana ({largo}) debe ser mayor que los retardos ({d0}, {d1})"
        )

    n_ventanas = x.size // largo
    ventanas: list[DiagnosticoVentana] = []
    for k in range(n_ventanas):
        ini = k * largo
        fin = ini + largo
        ventanas.append(
            evaluar_ventana(x[ini:fin], d0, d1, umbral_eco=umbral_eco, indice=k)
        )

    no_silenciosas = [v for v in ventanas if v.rms > UMBRAL_SILENCIO]
    con_eco = [v for v in no_silenciosas if v.bit != SIN_ECO]
    if not no_silenciosas or len(con_eco) < FRACCION_MIN_VENTANAS_CON_ECO * len(no_silenciosas):
        return ResultadoDecodificacion(bits=[], ventanas=ventanas)

    # Recortar cola final de ventanas consecutivas sin eco (p. ej. REPETIR_MENSAJE = False).
    fin_activo = len(ventanas)
    sin_eco_finales = 0
    for k in range(len(ventanas) - 1, -1, -1):
        if ventanas[k].bit == SIN_ECO:
            sin_eco_finales += 1
        else:
            break
    if sin_eco_finales >= 4:
        fin_activo = len(ventanas) - sin_eco_finales

    bits: list[int] = []
    for k in range(fin_activo):
        v = ventanas[k]
        if v.rms <= UMBRAL_SILENCIO:
            continue
        bits.append(0 if v.r0 >= v.r1 else 1)

    return ResultadoDecodificacion(bits=bits, ventanas=ventanas)


def decodificar_por_us(
    senal: Sequence[float | int] | npt.NDArray[Any],
    retardo0_us: int = p.RETARDO0_US,
    retardo1_us: int = p.RETARDO1_US,
    umbral_eco: float = UMBRAL_ECO,
) -> ResultadoDecodificacion:
    """Conveniencia: convierte retardos de microsegundos a muestras y decodifica."""
    d0 = p.muestras_desde_us(retardo0_us)
    d1 = p.muestras_desde_us(retardo1_us)
    largo = p.largo_ventana(retardo0_us, retardo1_us)
    return decodificar_senal(senal, d0, d1, largo_ventana=largo, umbral_eco=umbral_eco)
