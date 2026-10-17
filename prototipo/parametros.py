"""
Definición de una serie de parámetros usados a lo largo del proyecto. 
Usa nombres y reglas equivalentes a las de  src/parametros.hpp.

Las conversiones usan aritmética entera o la misma operación en coma flotante,
para que Python y C++ den los mismos números (prototipo/vectores_parametros.py).
Los valores por defecto (R18) y K_VENTANA (R33, R42) son provisionales. Los valores
finales serán definidos experimentalmente más adelante.
"""

import math
from typing import Any, overload

import numpy as np
import numpy.typing as npt

FS = 44100  # Hz (R25)

# Categorías de metadatos (R10-R16) y su nombre para notificar (R49).
TITULO, ARTISTA, AUTOR, ALBUM, ANIO, INVITADO, PRODUCTOR = range(1, 8)
NOMBRES = {
    TITULO: "Título",
    ARTISTA: "Artista",
    AUTOR: "Autor",
    ALBUM: "Álbum",
    ANIO: "Año",
    INVITADO: "Artista invitado",
    PRODUCTOR: "Productor",
}

# Retardos, en microsegundos (R19-R21).
RETARDO_MIN_US = 1
RETARDO_MAX_US = 1_000_000
RETARDO_MIN_UTIL_US = 12  # el menor que da al menos 1 muestra


def muestras_desde_us(t_us: int | np.integer) -> int:
    """Retardo en us -> muestras; la media muestra redondea hacia arriba.
    Con enteros porque round() de Python y std::round de C++ redondean distinto."""
    if not isinstance(t_us, (int, np.integer)) or isinstance(t_us, bool):
        raise TypeError(f"el retardo debe ser entero, no {type(t_us).__name__}")
    if not RETARDO_MIN_US <= t_us <= RETARDO_MAX_US:
        raise ValueError(f"retardo fuera de [{RETARDO_MIN_US}, {RETARDO_MAX_US}] us: {t_us}")
    return (int(t_us) * FS + 500_000) // 1_000_000


def amplitud_desde_porcentaje(porcentaje: float) -> float:
    """Atenuación en % (R22-R24) -> amplitud a_b del eco en H_b(z) = 1 + a_b z^(-t_b).
    Con ATENUACION_ES_AMPLITUD, a_b = p/100; si no, a_b = 1 - p/100. El rango
    (0, 100) es el que acepta el codificador."""
    if not 0 < porcentaje < 100:
        raise ValueError(f"atenuación fuera de (0, 100) %: {porcentaje}")
    return porcentaje / 100.0 if ATENUACION_ES_AMPLITUD else 1.0 - porcentaje / 100.0


@overload
def a_flotante(m: int) -> float: ...
@overload
def a_flotante(m: npt.NDArray[Any]) -> npt.NDArray[np.float64]: ...


def a_flotante(m: int | npt.NDArray[Any]) -> float | npt.NDArray[np.float64]:
    """int16 -> [-1, 1). Acepta un entero o un arreglo de NumPy."""
    if isinstance(m, np.ndarray):
        return m.astype(np.float64) / 32768.0
    return m / 32768.0


@overload
def a_int16(y: float) -> int: ...
@overload
def a_int16(y: npt.NDArray[Any]) -> npt.NDArray[np.int16]: ...


def a_int16(y: float | npt.NDArray[Any]) -> int | npt.NDArray[np.int16]:
    """[-1, 1] -> int16: y*32768 saturado a [-32768, 32767] (el eco suma amplitud) y
    truncado hacia cero, como el escritor de WAV del codificador. Acepta un real
    (devuelve int) o un arreglo de NumPy (np.int16). NaN o infinito: ValueError."""
    if isinstance(y, np.ndarray):
        y = y.astype(np.float64)
        if not np.isfinite(y).all():
            raise ValueError("a_int16: hay muestras NaN o infinitas")
        return np.clip(y * 32768.0, -32768, 32767).astype(np.int16)
    if not math.isfinite(y):
        raise ValueError(f"a_int16: muestra no finita: {y}")
    return math.trunc(min(max(float(y) * 32768.0, -32768.0), 32767.0))


# Valores por defecto (R17, R18), provisionales.
RETARDO0_US = 1000   # 44 muestras
RETARDO1_US = 1500   # 66 muestras
ATENUACION0 = 30.0  
ATENUACION1 = 30.0  

# Ventana = K veces el mayor retardo (R33, R42). Provisional.
K_VENTANA = 25


def largo_ventana(t0_us: int, t1_us: int) -> int:
    """Largo de la ventana en muestras."""
    return K_VENTANA * max(muestras_desde_us(t0_us), muestras_desde_us(t1_us))


# Bits distintos que se aceptan en un preámbulo esperado (trama_registros.py).
TOLERANCIA_PREAMBULO = 1

# Estas siguientes son decisiones de diseño. Algunas vienen de ambiguedades en
# el enunciado.

# R22 Esta viene de la incertidumbre sobre lo que quiere decir atenuación aquí: 
# True -> la atenuación es la amplitud del eco (30 % -> a = 0.3);
# False -> el eco se atenúa ese porcentaje (30 % -> a = 0.7).
ATENUACION_ES_AMPLITUD = True  # Por el momento se asume que hablamos de la amplitud del eco

# R31 Que hacer si no cabe ni una copia del mensaje: "truncar" (codifica lo que cabe y
# avisa) o simplemente devolver un error.
SI_NO_CABE = "truncar" # Truncar por ahora (mejor tener algo del mensaje que nada, asumo)

# True: el mensaje se repite hasta llenar las ventanas; False: va una vez y las
# demás ventanas quedan sin eco.
REPETIR_MENSAJE = True

# Formato de la trama (metadatos.FORMATOS).
FORMATO_TRAMA = "registros"

# R50 True -> desconocidos y dañados solo se avisan si hubo algún registro
# válido (un archivo sin codificar no da avisos); False -> se avisan siempre.
AVISOS_REQUIEREN_VALIDO = True
