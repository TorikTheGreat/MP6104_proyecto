"""
Línea de comandos para el programa final, ecoesteg (sección 5.2 del enunciado).

Para tener una clara separación de las posibles fuentes de error, este módulo contiene
funciones para validar la configuración que se recibe de entrada. Será utilizado por
el script que integrará el prototipo de punta a punta, validando cada entrada para evitar 
una ejecución inválida.

analizar() trabaja sobre bytes porque sys.argv esconde los bytes que no son
UTF-8 válido (se pasan con os.fsencode). Los números se validan con su forma antes
de convertirlos, para que Python y C++ acepten exactamente lo mismo; el port a C++
se compara con prototipo/vectores_linea_comandos.py. Los retardos y atenuaciones
no dados quedan en None y resolver() les pone los valores por defecto.
"""

import os
import re
import sys
from dataclasses import dataclass, field
from typing import Sequence

import parametros as p

EXITO = 0
ERROR_EJECUCION = 1  # archivo, formato WAV; mensaje que no cabe si SI_NO_CABE == "error"
ERROR_USO = 2        # R7 (parámetros desconocidos) y valores inválidos

USO = """\
Uso:
  ecoesteg ENTRADA.wav SALIDA.wav [metadatos] [enmascaramiento] [otras]   codifica
  ecoesteg ENTRADA.wav [enmascaramiento] [otras]                          decodifica

Metadatos (texto UTF-8 de 1 a 255 bytes; con al menos uno, se codifica):
  --titulo T  --artista T  --autor T  --album T  --anio T  --invitado T  --productor T

Enmascaramiento (los mismos al codificar y al decodificar):
  --retardo0 US      retardo del eco del bit 0, en us (12 a 1000000)    [{r0}]
  --retardo1 US      retardo del eco del bit 1, en us (12 a 1000000)    [{r1}]
  --atenuacion0 P    amplitud del eco del bit 0, en % (0, 100)          [{a0:g}]
  --atenuacion1 P    amplitud del eco del bit 1, en % (0, 100)          [{a1:g}]

Otras:
  --canal C          izquierdo o derecho                                [izquierdo]
  --diagnostico F    escribe en F un CSV con el bit de cada ventana
  --ayuda            muestra esta ayuda

Códigos de salida: 0 éxito, 1 error de ejecución, 2 error de uso.
""".format(r0=p.RETARDO0_US, r1=p.RETARDO1_US, a0=p.ATENUACION0, a1=p.ATENUACION1)

METADATOS = {
    b"--titulo": p.TITULO,
    b"--artista": p.ARTISTA,
    b"--autor": p.AUTOR,
    b"--album": p.ALBUM,
    b"--anio": p.ANIO,
    b"--invitado": p.INVITADO,
    b"--productor": p.PRODUCTOR,
}
ORDEN = list(METADATOS) + [
    b"--retardo0", b"--retardo1", b"--atenuacion0", b"--atenuacion1",
    b"--canal", b"--diagnostico",
]
CON_VALOR = set(ORDEN)
CANALES = (b"izquierdo", b"derecho")

# [0-9] y no \d, que acepta dígitos no ASCII; fullmatch y no ^...$, que acepta
# un salto de línea final.
FORMA_RETARDO = re.compile(rb"[0-9]{1,7}")
FORMA_ATENUACION = re.compile(rb"[0-9]{1,3}(\.[0-9]{1,6})?")


@dataclass
class Configuracion:
    modo: str = ""                 # "codificar", "decodificar" o "ayuda"
    entrada: bytes = b""
    salida: bytes = b""            # vacía al decodificar
    metadatos: dict[int, bytes] = field(default_factory=dict)  # categoría -> bytes UTF-8
    retardo0_us: int | None = None
    retardo1_us: int | None = None
    atenuacion0: float | None = None  # %
    atenuacion1: float | None = None
    canal: str = "izquierdo"
    diagnostico: bytes | None = None


@dataclass
class ErrorUso:
    mensaje: str


def _mostrar(b: bytes) -> str:
    return b.decode("utf-8", errors="backslashreplace")


def _es_opcion(a: bytes) -> bool:
    return a.startswith(b"-") and a != b"-"


def analizar(args: Sequence[bytes]) -> Configuracion | ErrorUso:
    """args: lista de bytes (sin el nombre del programa) -> Configuracion o ErrorUso."""
    c = Configuracion()
    valores: dict[bytes, bytes] = {}  # opción -> último valor (R9)
    posicionales: list[bytes] = []
    ayuda = False
    solo_posicionales = False
    i = 0
    while i < len(args):
        a = args[i]
        i += 1
        if solo_posicionales or not _es_opcion(a):
            posicionales.append(a)
            continue
        if a == b"--":
            solo_posicionales = True
            continue
        nombre, igual, valor = a.partition(b"=")
        if nombre == b"--ayuda":
            if igual:
                return ErrorUso("--ayuda no lleva valor")
            ayuda = True
            continue
        if nombre not in CON_VALOR:
            return ErrorUso(f"opción desconocida: {_mostrar(nombre)}")
        if not igual:
            if i >= len(args) or _es_opcion(args[i]):
                return ErrorUso(f"falta el valor de {_mostrar(nombre)}")
            valor = args[i]
            i += 1
        valores[nombre] = valor

    for nombre in ORDEN:  # orden fijo: el mismo primer error en Python y C++
        if nombre in valores:
            error = _asignar(c, nombre, valores[nombre])
            if error:
                return ErrorUso(error)

    if ayuda:
        c.modo = "ayuda"
        return c
    if c.metadatos:
        if len(posicionales) != 2:
            return ErrorUso("para codificar se necesitan ENTRADA.wav y SALIDA.wav")
        c.modo, c.entrada, c.salida = "codificar", posicionales[0], posicionales[1]
        if c.entrada == c.salida:
            return ErrorUso("SALIDA.wav no puede ser la misma ruta que ENTRADA.wav")
    else:
        if len(posicionales) != 1:
            return ErrorUso("para decodificar se necesita solo ENTRADA.wav "
                            "(para codificar, al menos un metadato)")
        c.modo, c.entrada = "decodificar", posicionales[0]

    r0, r1, _, _ = resolver(c)
    if p.muestras_desde_us(r0) == p.muestras_desde_us(r1):
        return ErrorUso(f"los retardos {r0} y {r1} us dan las mismas "
                        f"{p.muestras_desde_us(r0)} muestras; deben ser distintos")
    return c


def _asignar(c: Configuracion, nombre: bytes, valor: bytes) -> str | None:
    """Valida y guarda una opción; devuelve el mensaje de error o None."""
    texto = _mostrar(nombre)
    if nombre in METADATOS:
        try:
            valor.decode("utf-8")
        except UnicodeDecodeError:
            return f"{texto}: el texto no es UTF-8 válido"
        if not 1 <= len(valor) <= 255:
            return f"{texto}: el texto debe tener de 1 a 255 bytes (tiene {len(valor)})"
        c.metadatos[METADATOS[nombre]] = valor
    elif nombre in (b"--retardo0", b"--retardo1"):
        if not FORMA_RETARDO.fullmatch(valor):
            return f'{texto}: se esperaba un entero de us, no "{_mostrar(valor)}"'
        t = int(valor)
        if not p.RETARDO_MIN_UTIL_US <= t <= p.RETARDO_MAX_US:
            return (f"{texto}: {t} us está fuera de rango ({p.RETARDO_MIN_UTIL_US} a "
                    f"{p.RETARDO_MAX_US} us, al menos 1 muestra)")
        setattr(c, "retardo0_us" if nombre == b"--retardo0" else "retardo1_us", t)
    elif nombre in (b"--atenuacion0", b"--atenuacion1"):
        if not FORMA_ATENUACION.fullmatch(valor):
            return f'{texto}: se esperaba un porcentaje como 30 o 12.5, no "{_mostrar(valor)}"'
        entero, _, decimales = valor.partition(b".")
        millonesimas = int(entero) * 1_000_000 + int(decimales.ljust(6, b"0"))
        if not 0 < millonesimas < 100_000_000:
            return f"{texto}: {_mostrar(valor)} % está fuera de (0, 100)"
        a = millonesimas / 1e6
        setattr(c, "atenuacion0" if nombre == b"--atenuacion0" else "atenuacion1", a)
    elif nombre == b"--canal":
        if valor not in CANALES:
            return f'--canal: se esperaba izquierdo o derecho, no "{_mostrar(valor)}"'
        c.canal = valor.decode()
    elif nombre == b"--diagnostico":
        if not valor:
            return "--diagnostico: falta la ruta"
        c.diagnostico = valor
    return None


def resolver(c: Configuracion) -> tuple[int, int, float, float]:
    """(retardo0_us, retardo1_us, atenuacion0, atenuacion1) con los por defecto."""
    return (
        p.RETARDO0_US if c.retardo0_us is None else c.retardo0_us,
        p.RETARDO1_US if c.retardo1_us is None else c.retardo1_us,
        p.ATENUACION0 if c.atenuacion0 is None else c.atenuacion0,
        p.ATENUACION1 if c.atenuacion1 is None else c.atenuacion1,
    )


def principal(argv: Sequence[str] | None = None) -> int:
    """Analiza sys.argv y muestra la configuración (para probar a mano)."""
    args = [os.fsencode(a) for a in (sys.argv[1:] if argv is None else argv)]
    r = analizar(args)
    if isinstance(r, ErrorUso):
        print(f"error: {r.mensaje}\n\n{USO}", file=sys.stderr, end="")
        return ERROR_USO
    if r.modo == "ayuda":
        print(USO, end="")
        return EXITO
    print(r)
    return EXITO


if __name__ == "__main__":
    sys.exit(principal())
