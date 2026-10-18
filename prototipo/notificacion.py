"""
Notificación de metadatos (sección 4.3.4, R48-R51) y avisos del codificador.

Recibe los metadatos después de que ya fueron interpretados y arma dos listas:
una con los metadatos válidos, que van a la salida standard con su categoría, 
y una con los desconocidos o dañados, que van a la salida de error con una 
advertencia.

Independiente del formato de la trama: recibe la lista de resultados de
metadatos.desde_bits.
"""

from collections.abc import Iterable

from parametros import FS, NOMBRES
from trama import DANADO, DESCONOCIDO, VALIDO, Lectura


def _largo_secuencia_utf8(datos: bytes, i: int) -> int:
    """Largo de la secuencia UTF-8 válida que empieza en datos[i], o 0.
    Tabla 3-7 del estándar Unicode: rechaza sobrelargas, sustitutos y valores
    mayores que U+10FFFF."""
    b = datos[i]
    if b < 0x80:
        return 1
    if 0xC2 <= b <= 0xDF:
        rangos = [(0x80, 0xBF)]
    elif b == 0xE0:
        rangos = [(0xA0, 0xBF), (0x80, 0xBF)]
    elif 0xE1 <= b <= 0xEC or 0xEE <= b <= 0xEF:
        rangos = [(0x80, 0xBF), (0x80, 0xBF)]
    elif b == 0xED:
        rangos = [(0x80, 0x9F), (0x80, 0xBF)]
    elif b == 0xF0:
        rangos = [(0x90, 0xBF), (0x80, 0xBF), (0x80, 0xBF)]
    elif 0xF1 <= b <= 0xF3:
        rangos = [(0x80, 0xBF)] * 3
    elif b == 0xF4:
        rangos = [(0x80, 0x8F), (0x80, 0xBF), (0x80, 0xBF)]
    else:
        return 0
    if i + len(rangos) >= len(datos):  # secuencia truncada
        return 0
    for k, (bajo, alto) in enumerate(rangos, start=1):
        if not bajo <= datos[i + k] <= alto:
            return 0
    return 1 + len(rangos)


def texto_visible(datos: bytes) -> str:
    """Cada byte que no forma parte de una secuencia UTF-8 válida se imprime como
    "?". Se hace a mano para que C++ dé lo mismo (errors="replace" agrupa bytes)."""
    partes = []
    i = 0
    while i < len(datos):
        largo = _largo_secuencia_utf8(datos, i)
        if largo:
            partes.append(datos[i:i + largo].decode("utf-8"))
            i += largo
        else:
            partes.append("?")
            i += 1
    return "".join(partes)


def notificar(resultados: Iterable[Lectura]) -> tuple[list[str], list[str]]:
    """Líneas para la salida estándar y para la de error."""
    salida: list[str] = []
    errores: list[str] = []
    for tipo, categoria, datos in resultados:
        texto = texto_visible(datos)
        if tipo == VALIDO:
            salida.append(f"{NOMBRES[categoria]}: {texto}")
        elif tipo == DANADO:
            # Sin categoría: el byte de categoría está bajo el CRC que falló.
            errores.append(f"ADVERTENCIA: metadato desconocido (dañado): {texto}")
        elif tipo == DESCONOCIDO:
            errores.append(f"ADVERTENCIA: metadato desconocido (categoría {categoria}): {texto}")
    if not resultados:
        errores.append("ADVERTENCIA: no se encontraron metadatos")
    return salida, errores


def aviso_no_cabe(necesarios: int, disponibles: int, largo_ventana: int) -> list[str]:
    """Aviso del codificador cuando el mensaje no cabe y se trunca (R31)."""
    faltan_s = (necesarios - disponibles) * largo_ventana / FS
    return [
        "*" * 72,
        "ADVERTENCIA: los metadatos no caben en el audio.",
        f"Se codificaron {disponibles} de {necesarios} bits; el resto se perdió.",
        f"Para codificarlos completos faltan unos {faltan_s:.1f} s de audio.",
        "*" * 72,
    ]
