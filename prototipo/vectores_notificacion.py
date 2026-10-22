"""
Genera vectores de prueba para la decodificación y notificación. La implementación en C++
luego los usará para asegurar la paridad de comportamiento con respecto a los prototipos
de python.

Vectores de la decodificación y la notificación: bits pasados por el canal
simulado y lo que se imprime (metadatos.desde_bits, notificacion.py), y la regla
de los "?". Los usa el port a C++."""

import metadatos as m
import notificacion as n
import trama_registros as f
from canal_simulado import canal, generador
from vectores import escribir
from vectores_metadatos import SIETE

SEMILLA = 7

# (nombre, p_inversion, p_borrado, p_insercion); cada uno con su propia semilla.
CANALES = [
    ("limpio", 0.0, 0.0, 0.0),
    ("inversion_1", 0.01, 0.0, 0.0),
    ("inversion_5", 0.05, 0.0, 0.0),
    ("borrado", 0.0, 0.005, 0.0),
    ("insercion", 0.0, 0.0, 0.005),
]


def generar(directorio):
    def esc(nombre, tipo, valores):
        escribir(directorio, f"notificacion_{nombre}", tipo, valores,
                 generador=__file__, semilla=SEMILLA)

    copia = f.codificar(SIETE)
    ventanas = 4 * len(copia) + 77
    esc("aviso_no_cabe", "texto", n.aviso_no_cabe(len(copia), 300, 2112))

    for k, (nombre, inv, bor, ins) in enumerate(CANALES):
        bits = canal(m.a_bits(SIETE, ventanas).bits, generador(SEMILLA + k),
                     p_inversion=inv, p_borrado=bor, p_insercion=ins)
        salida, errores = n.notificar(m.desde_bits(bits))
        esc(f"canal_{nombre}_bits", "entero", bits)
        esc(f"canal_{nombre}_salida", "texto", salida)
        esc(f"canal_{nombre}_errores", "texto", errores)

    crudos = [b"", "Ñandú".encode(), b"a\xe2\x82", b"\xc0\xaf", b"\xed\xa0\x80",
              b"\xf4\x90\x80\x80", b"\xf0\x9f\x8e\xb5x", b"\x80\xbf", b"\xe0\x80\x80",
              b"\xf5\x80", b"\xc2", bytes(range(0x80, 0x100)),
              b"\xf0\x80\x80\x80", b"\xf3\x80\x80\x80", b"\xef\xbf\xbd",
              b"a\nb\x1b[31mc\x7fd\te"]
    esc("visible_entradas", "texto", crudos)
    esc("visible_salidas", "texto", [n.texto_visible(c) for c in crudos])
