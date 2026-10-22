"""Metadatos <-> bits, independiente del formato de la trama.

Codificar: el formato (parametros.FORMATO_TRAMA) produce una copia del mensaje;
aquí se repite hasta llenar las ventanas disponibles o se trunca si no cabe.
Decodificar: el formato lee los registros; aquí se decide qué se reporta.
"""

from collections import Counter
from collections.abc import Iterable, Sequence
from dataclasses import dataclass
from types import ModuleType

import parametros as p
import trama_registros
import votacion
from trama import DANADO, DESCONOCIDO, VALIDO, Bits, Lectura, Metadatos

FORMATOS: dict[str, ModuleType] = {"registros": trama_registros}


def formato() -> ModuleType:
    return FORMATOS[p.FORMATO_TRAMA]


class NoCabe(Exception):
    """El mensaje no cabe y parametros.SI_NO_CABE es "error"."""

    def __init__(self, necesarios: int, disponibles: int) -> None:
        super().__init__(f"los metadatos necesitan {necesarios} bits y caben {disponibles}")
        self.necesarios = necesarios
        self.disponibles = disponibles


@dataclass
class Codificacion:
    bits: Bits          # un bit por ventana, en orden; puede ser más corto que las ventanas
    necesarios: int     # bits de una copia completa del mensaje
    truncado: bool      # True si no cupo una copia completa


def bits_necesarios(metadatos: Metadatos) -> int:
    return len(formato().codificar(metadatos))


def repetir(bits: Sequence[int], n: int) -> Bits:
    """Repite bits cíclicamente hasta tener exactamente n."""
    if n > 0 and not bits:
        raise ValueError("no hay bits que repetir")
    return [bits[i % len(bits)] for i in range(n)]


def a_bits(metadatos: Metadatos, n_ventanas: int) -> Codificacion:
    """Bits para las n_ventanas que pueden llevar uno.

    Con REPETIR_MENSAJE el mensaje se repite hasta llenarlas; si no, va una vez y
    las ventanas sobrantes quedan sin eco. Si no cabe ni una copia: con
    SI_NO_CABE = "truncar" se codifica lo que cabe (quien llama avisa con
    notificacion.aviso_no_cabe); con "error" se lanza NoCabe.
    """
    copia = formato().codificar(metadatos)
    if len(copia) > n_ventanas:
        if p.SI_NO_CABE == "error":
            raise NoCabe(len(copia), n_ventanas)
        return Codificacion(copia[:n_ventanas], len(copia), True)
    bits = repetir(copia, n_ventanas) if p.REPETIR_MENSAJE else copia
    return Codificacion(bits, len(copia), False)


def leer_votada(bits: Sequence[int]) -> list[Lectura]:
    """Registros de la copia votada, cada uno una vez. La copia se lee dos veces
    seguidas porque es cíclica: un registro puede empezar al final y seguir al
    principio."""
    votada = votacion.votar_copias(bits)
    if votada is None:
        return []
    lecturas: list[Lectura] = []
    for lectura in formato().leer(votada + votada):
        if lectura not in lecturas:
            lecturas.append(lectura)
    return lecturas


def desde_bits(bits: Iterable[int]) -> list[Lectura]:
    """Metadatos recuperados, sin duplicados, en orden de primera aparición.

    bits: los de las ventanas clasificadas como 0 o 1. Devuelve una lista de
    (tipo, categoría, datos):
    - por cada categoría válida, el texto que aparece en más copias (empate: el
      primero);
    - cada desconocido o dañado distinto, una vez. Un dañado no se reporta si su
      categoría tiene una copia válida (es una copia dañada de algo ya reportado).
    Con AVISOS_REQUIEREN_VALIDO, desconocidos y dañados solo se reportan si hubo
    algún registro válido: un archivo sin codificar no produce avisos.
    Con VOTAR_COPIAS, los registros de la copia votada (votacion.py) se suman como
    una copia más.
    """
    bits = list(bits)
    lecturas = formato().leer(bits)
    if p.VOTAR_COPIAS:
        lecturas += leer_votada(bits)

    copias_validas: dict[int, Counter[bytes]] = {}  # textos por categoría, en orden
    for tipo, categoria, datos in lecturas:
        if tipo == VALIDO:
            copias_validas.setdefault(categoria, Counter())[datos] += 1
    avisar = bool(copias_validas) or not p.AVISOS_REQUIEREN_VALIDO

    resultados: list[Lectura] = []
    vistos: set[tuple[object, ...]] = set()
    for tipo, categoria, datos in lecturas:
        clave: tuple[object, ...]
        if tipo == VALIDO:
            clave = (VALIDO, categoria)
            datos = copias_validas[categoria].most_common(1)[0][0]
        elif tipo == DANADO:
            if not avisar or categoria in copias_validas:
                continue
            clave = (DANADO, datos)
        else:
            if not avisar:
                continue
            clave = (DESCONOCIDO, categoria, datos)
        if clave not in vistos:
            vistos.add(clave)
            resultados.append((tipo, categoria, datos))
    return resultados
