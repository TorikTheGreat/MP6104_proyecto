"""Formato de trama "registros": un registro por metadato.

    preámbulo   8 bits    0xB2
    categoría   8 bits    1-7
    longitud    8 bits    n, bytes del texto en UTF-8 (1-255)
    datos       8n bits   el texto en UTF-8
    CRC-16      16 bits   CRC-16/CCITT-FALSE sobre categoría, longitud y datos

Cada byte va con el bit más significativo primero. Una copia del mensaje son los
registros en orden de categoría. Implementa la interfaz de trama.py.
"""

from collections.abc import Iterable, Sequence

from parametros import NOMBRES, TOLERANCIA_PREAMBULO
from trama import DANADO, DESCONOCIDO, VALIDO, Bits, Lectura, Metadatos

PREAMBULO = 0xB2
BITS_SOBRECARGA = 40
LONGITUD_MAX = 255


def crc16(datos: bytes) -> int:
    """CRC-16/CCITT-FALSE: polinomio 0x1021, valor inicial 0xFFFF.
    crc16(b"123456789") == 0x29B1."""
    crc = 0xFFFF
    for byte in datos:
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) if crc & 0x8000 else (crc << 1)
            crc &= 0xFFFF
    return crc


def bits_de_bytes(datos: bytes) -> Bits:
    return [(byte >> (7 - k)) & 1 for byte in datos for k in range(8)]


def _byte(bits: Sequence[int], i: int) -> int:
    valor = 0
    for b in bits[i:i + 8]:
        valor = (valor << 1) | b
    return valor


def _como_bytes(texto: str | bytes) -> bytes:
    return texto.encode("utf-8") if isinstance(texto, str) else bytes(texto)


def codificar(metadatos: Metadatos) -> Bits:
    """metadatos: {categoría: texto (str o bytes UTF-8)}."""
    if not metadatos:
        raise ValueError("no hay metadatos que codificar")
    bits: Bits = []
    for categoria in sorted(metadatos):
        if categoria not in NOMBRES:
            raise ValueError(f"categoría inválida: {categoria}")
        datos = _como_bytes(metadatos[categoria])
        if not 1 <= len(datos) <= LONGITUD_MAX:
            raise ValueError(f"{NOMBRES[categoria]}: {len(datos)} bytes (válido: 1 a 255)")
        cuerpo = bytes([categoria, len(datos)]) + datos
        crc = crc16(cuerpo)
        bits += bits_de_bytes(bytes([PREAMBULO]) + cuerpo + bytes([crc >> 8, crc & 0xFF]))
    return bits


def _utf8_valido(datos: bytes) -> bool:
    try:
        datos.decode("utf-8")
        return True
    except UnicodeDecodeError:
        return False


def leer(bits: Iterable[int]) -> list[Lectura]:
    """Registros leídos en bits, en orden, con repetidos.

    Donde terminó el último registro con CRC correcto se acepta un preámbulo con
    hasta TOLERANCIA_PREAMBULO bits distintos; en cualquier otra posición, solo uno
    exacto. Si el registro no se confirma, se busca desde el bit siguiente: así un
    bit borrado o insertado cuesta un registro. Un CRC incorrecto se anota como
    DANADO solo si el registro empezaba donde terminó el anterior; los demás suelen
    ser falsos preámbulos dentro de los datos.
    """
    bits = list(bits)
    n = len(bits)
    lecturas: list[Lectura] = []
    esperado = 0  # fin del último registro con CRC correcto
    inicio = 0
    while inicio + BITS_SOBRECARGA + 8 <= n:
        en_secuencia = inicio == esperado
        tolerancia = TOLERANCIA_PREAMBULO if en_secuencia else 0
        if bin(_byte(bits, inicio) ^ PREAMBULO).count("1") > tolerancia:
            inicio += 1
            continue
        categoria = _byte(bits, inicio + 8)
        longitud = _byte(bits, inicio + 16)
        fin = inicio + BITS_SOBRECARGA + 8 * longitud
        if longitud == 0 or fin > n:
            inicio += 1
            continue
        cuerpo = bytes(_byte(bits, inicio + 8 + 8 * k) for k in range(2 + longitud))
        crc = (_byte(bits, fin - 16) << 8) | _byte(bits, fin - 8)
        datos = cuerpo[2:]
        if crc16(cuerpo) == crc:
            tipo = VALIDO if categoria in NOMBRES and _utf8_valido(datos) else DESCONOCIDO
            lecturas.append((tipo, categoria, datos))
            esperado = inicio = fin
            continue
        if en_secuencia and categoria in NOMBRES:
            lecturas.append((DANADO, categoria, datos))
        inicio += 1
    return lecturas
