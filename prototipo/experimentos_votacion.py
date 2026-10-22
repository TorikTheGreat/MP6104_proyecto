"""
Corre una serie de experimentos comparando ejecuciones con y sin la votación;
luego compila los metadatos recuperados de ambas formas, tanto con el canal 
simulado como con las grabaciones reales de datos/ y escribe los conteos de
cada uno en resultados/votacion.csv.
"""

import argparse
import csv
from pathlib import Path
from unittest import mock

import numpy as np

import decodificador as dec
import experimentos_recuperacion as er
import metadatos as m
import parametros as p
from canal_simulado import canal, generador
from vectores_metadatos import SIETE

RAIZ = Path(__file__).resolve().parent.parent
SEMILLAS = 20
VENTANAS_3_MIN = 180 * p.FS // p.largo_ventana(p.RETARDO0_US, p.RETARDO1_US)
CANALES = [
    ("inversion", 0.00, 0.0, 0.0),
    ("inversion", 0.01, 0.0, 0.0),
    ("inversion", 0.02, 0.0, 0.0),
    ("inversion", 0.05, 0.0, 0.0),
    ("inversion", 0.10, 0.0, 0.0),
    ("borrado", 0.0, 0.005, 0.0),
    ("insercion", 0.0, 0.0, 0.005),
]
REALES = ["clasica_vivaldi.wav", "ragtime_joplin.wav", "jazz_park.wav"]
TRES = {p.TITULO: "Canción de prueba", p.ARTISTA: "Banda", p.ANIO: "2026"}


def validos(bits):
    return sum(1 for tipo, _, _ in m.desde_bits(bits) if tipo == "valido")


def con_y_sin_voto(bits):
    with mock.patch.object(p, "VOTAR_COPIAS", False):
        sin = validos(bits)
    return sin, validos(bits)


def fila(fuente, condicion, copias, categorias, sin, con):
    return {"fuente": fuente, "condicion": condicion, "copias": round(copias, 2),
            "categorias": categorias, "validas_sin_voto": round(sin, 2),
            "validas_con_voto": round(con, 2)}


def canal_simulado():
    mensaje = m.a_bits(SIETE, VENTANAS_3_MIN).bits
    copias = VENTANAS_3_MIN / m.bits_necesarios(SIETE)
    filas = []
    for k, (tipo, inv, bor, ins) in enumerate(CANALES):
        resultados = [con_y_sin_voto(canal(mensaje, generador(1000 * k + s), inv, bor, ins))
                      for s in range(SEMILLAS)]
        sin, con = np.mean(resultados, axis=0)
        condicion = f"{tipo} {100 * (inv or bor or ins):g} %"
        filas.append(fila("canal simulado", condicion, copias, len(SIETE), sin, con))
        print(f"{condicion:16s} sin voto {sin:.2f}/7  con voto {con:.2f}/7")
    return filas


def grabaciones_reales(datos):
    if not all((datos / a).exists() for a in REALES):
        print("Faltan los WAV de datos/: se omiten las grabaciones reales.")
        return []
    x = np.concatenate([er.leer_canal_izquierdo(datos / a) for a in REALES])
    d0, d1 = p.muestras_desde_us(p.RETARDO0_US), p.muestras_desde_us(p.RETARDO1_US)
    largo = p.largo_ventana(p.RETARDO0_US, p.RETARDO1_US)
    a = p.amplitud_desde_porcentaje(p.ATENUACION0)
    n = x.size // largo
    bits = m.a_bits(TRES, n).bits
    y = er.codificar_solapamiento_y_suma(x, bits, d0, d1, a, a, largo)
    leidos = dec.decodificar_senal(y, d0, d1, largo_ventana=largo).bits
    acierto = 100 * np.mean(np.array(leidos) == np.array(bits))
    sin, con = con_y_sin_voto(leidos)
    print(f"grabaciones reales: {acierto:.2f} % de bits, sin voto {sin}/3, con voto {con}/3")
    return [fila("grabaciones reales 60 s", f"{acierto:.2f} % de bits", n / len(m.formato().codificar(TRES)),
                 len(TRES), sin, con)]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--datos", type=Path, default=RAIZ / "datos")
    parser.add_argument("--resultados", type=Path, default=RAIZ / "resultados")
    parser.add_argument("--programa", type=Path, default=RAIZ / "bin" / "ecoesteg")  # no se usa
    args = parser.parse_args()
    filas = canal_simulado() + grabaciones_reales(args.datos)
    args.resultados.mkdir(parents=True, exist_ok=True)
    with open(args.resultados / "votacion.csv", "w", newline="", encoding="utf-8") as f:
        escritor = csv.DictWriter(f, fieldnames=list(filas[0]))
        escritor.writeheader()
        escritor.writerows(filas)
    print(f"Tabla guardada: {args.resultados / 'votacion.csv'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
