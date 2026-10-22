"""Elección de K, el factor de la ventana L = K max(d0, d1) (R33, R42).

El enunciado pide la ventana del menor tamaño posible. Con los retardos y la
atenuación por defecto, compila bin/ecoesteg con cada K de la lista y mide lo
mismo que la búsqueda de los valores por defecto: PESQ y recuperación de bits
promedio de cada pieza en los tramos de ajuste (ajuste_parametros.py), metadatos
recuperados en las canciones completas sin el tramo de prueba
(ajuste_canciones.py) y, como referencia, los fragmentos de prueba de datos/.

Necesita g++, las canciones de datos/originales/ (make datos y
ajuste_parametros.py) y pesq. Tarda unos 15 minutos. Escribe
resultados/ajuste_ventana.csv.

    python prototipo/ajuste_ventana.py
"""

import argparse
import math
import subprocess
import tempfile
from collections import defaultdict
from pathlib import Path

import ajuste_canciones as ac
import ajuste_parametros as ap
import experimentos_calidad as ec
import parametros as p

RAIZ = Path(__file__).resolve().parent.parent
K = [5, 8, 10, 12, 15, 20, 25]
PRUEBA = ["clasica_vivaldi.wav", "ragtime_joplin.wav", "jazz_park.wav"]


def compilar(k: int, carpeta: Path) -> Path:
    programa = carpeta / f"ecoesteg_k{k}"
    subprocess.run(["g++", "-O2", "-std=c++17", f"-DECOESTEG_K_VENTANA={k}",
                    *sorted(str(f) for f in (RAIZ / "src").glob("*.cpp")), "-o", programa],
                   check=True)
    return programa


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--resultados", type=Path, default=RAIZ / "resultados")
    args = parser.parse_args()
    if not all((ap.ORIGINALES / f"{c}.wav").exists() for c in ac.CANCIONES):
        print("Faltan las canciones completas: corra make datos y ajuste_parametros.py.")
        return 1
    r0, r1, a = p.RETARDO0_US, p.RETARDO1_US, round(p.ATENUACION0)

    filas = []
    with tempfile.TemporaryDirectory() as tmp:
        carpeta = Path(tmp)
        tramos = ap.cortar_tramos(carpeta)
        for k in K:
            programa = compilar(k, carpeta)
            pesq, recuperacion = defaultdict(list), defaultdict(list)
            for tramo in tramos:
                pieza = tramo.stem.rsplit("_", 1)[0]
                m = ec.medir(programa, tramo, (r0, r1), a, carpeta)
                if not math.isnan(m["pesq"]):
                    pesq[pieza].append(m["pesq"])
                recuperacion[pieza].append(m["recuperacion"])
            fila: dict = {"k": k, "largo_ventana": k * max(p.muestras_desde_us(r0),
                                                            p.muestras_desde_us(r1)),
                          "tasa_bps": round(p.FS / (k * max(p.muestras_desde_us(r0),
                                                             p.muestras_desde_us(r1))), 2)}
            fila["pesq_minimo"] = round(min(sum(v) / len(v) for v in pesq.values()), 3)
            fila["recuperacion_minima"] = round(
                min(sum(v) / len(v) for v in recuperacion.values()), 2)
            metadatos, peor = 0, 100.0
            for c in ac.CANCIONES:
                n, rec = ac.medir(programa, ac.sin_prueba(c, carpeta), r0, r1, a, carpeta)
                metadatos += n
                peor = min(peor, rec)
            fila["metadatos_canciones"] = metadatos
            fila["recuperacion_minima_canciones"] = round(peor, 2)
            for f in PRUEBA:
                m = ec.medir(programa, RAIZ / "datos" / f, (r0, r1), a, carpeta)
                nombre = f.split("_")[0]
                fila[f"pesq_{nombre}"] = m["pesq"]
                fila[f"recuperacion_{nombre}"] = m["recuperacion"]
            filas.append(fila)
            print(f"K = {k:2d}: PESQ mínimo {fila['pesq_minimo']:.2f}, recuperación mínima "
                  f"{fila['recuperacion_minima']:.1f} %, {metadatos} de "
                  f"{7 * len(ac.CANCIONES)} metadatos", flush=True)
    ec.escribir_csv(args.resultados / "ajuste_ventana.csv", filas)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
