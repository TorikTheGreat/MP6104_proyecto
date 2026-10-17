"""Vectores de la codificación de la trama: CRC y formato (trama_registros.py),
repetición y truncado (metadatos.py). Los usa el port a C++."""

import metadatos as m
import trama_registros as f
from parametros import ALBUM, ANIO, ARTISTA, AUTOR, INVITADO, PRODUCTOR, TITULO
from vectores import escribir

SEMILLA = 7

SIETE = {
    TITULO: "Canción de prueba",
    ARTISTA: "Ñandú y los Pingüinos",
    AUTOR: "José Pérez",
    ALBUM: "Álbum único",
    ANIO: "2026",
    INVITADO: "Zoë",
    PRODUCTOR: "Estudio Ártico",
}


def generar(directorio):
    def esc(nombre, tipo, valores):
        escribir(directorio, f"metadatos_{nombre}", tipo, valores,
                 generador=__file__, semilla=SEMILLA)

    textos = [b"123456789", b"", "Canción".encode(), bytes(range(256))]
    esc("crc_entradas", "texto", textos)
    esc("crc_salidas", "entero", [f.crc16(t) for t in textos])

    copia = f.codificar(SIETE)
    esc("siete_bits", "entero", copia)
    esc("siete_repetido_bits", "entero", m.a_bits(SIETE, 4 * len(copia) + 77).bits)
    esc("siete_truncado_bits", "entero", m.a_bits(SIETE, 300).bits)
