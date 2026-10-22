"""Segunda etapa de la búsqueda de los valores por defecto (R18): metadatos
recuperados en canciones completas.

Recuperar el 90 % de los bits en tramos de 20 s no garantiza recuperar los
metadatos. Para cada combinación que cumplió el 90 % en ajuste_parametros.py (y
para los valores por defecto anteriores, como referencia), codifica los siete
metadatos de prueba con bin/ecoesteg en las cinco canciones completas de
datos/originales/, las decodifica y cuenta los metadatos recuperados y la
recuperación de bits de cada canción. A cada canción se le quita antes el tramo
de 20 s que está en datos/, para no elegir con los fragmentos de prueba.

Necesita resultados/ajuste_parametros.csv y las canciones que descarga
ajuste_parametros.py. Tarda unos 5 minutos. Escribe resultados/ajuste_canciones.csv.

    python prototipo/ajuste_canciones.py
"""

import argparse
import csv
import subprocess
import tempfile
import wave
from pathlib import Path

import ajuste_parametros as ap
import experimentos_calidad as ec
import parametros as p
from linea_comandos import METADATOS
from vectores_metadatos import SIETE

RAIZ = Path(__file__).resolve().parent.parent
CANCIONES = list(ap.TRAMOS)
ANTERIORES = (1000, 1500, 30)  # valores por defecto antes de la búsqueda
OPCION = {categoria: opcion.decode() for opcion, categoria in METADATOS.items()}
# Inicio (s) del fragmento de prueba de 20 s de cada canción (scripts/obtener_datos.sh).
PRUEBA = {"vivaldi_full": 85, "joplin_full": 5, "jazz_full": 5}


def sin_prueba(cancion: str, carpeta: Path) -> Path:
    """La canción sin su fragmento de prueba, si tiene uno."""
    origen = ap.ORIGINALES / f"{cancion}.wav"
    if cancion not in PRUEBA:
        return origen
    with wave.open(str(origen), "rb") as w:
        parametros = w.getparams()
        cuadros = w.readframes(w.getnframes())
    bytes_por_cuadro = parametros.nchannels * parametros.sampwidth
    ini = PRUEBA[cancion] * p.FS * bytes_por_cuadro
    fin = (PRUEBA[cancion] + 20) * p.FS * bytes_por_cuadro
    destino = carpeta / f"{cancion}.wav"
    with wave.open(str(destino), "wb") as o:
        o.setparams(parametros)
        o.writeframes(cuadros[:ini] + cuadros[fin:])
    return destino


def combinaciones(ajuste: Path) -> list[tuple[int, int, int]]:
    with open(ajuste, encoding="utf-8") as f:
        filas = [(int(r["retardo0_us"]), int(r["retardo1_us"]), int(r["atenuacion"]))
                 for r in csv.DictReader(f) if float(r["recuperacion_minima"]) >= 90]
    return filas + [ANTERIORES] if ANTERIORES not in filas else filas


def medir(programa: Path, cancion: Path, r0: int, r1: int, a: int,
          carpeta: Path) -> tuple[int, float]:
    """Metadatos recuperados y porcentaje de bits leídos igual."""
    eco = ["--retardo0", str(r0), "--retardo1", str(r1)]
    metadatos = [x for c, texto in SIETE.items() for x in (OPCION[c], texto)]
    codificado = carpeta / "codificado.wav"
    subprocess.run([programa, cancion, codificado, *metadatos, *eco,
                    "--atenuacion0", str(a), "--atenuacion1", str(a),
                    "--diagnostico", carpeta / "codificacion.csv"],
                   check=True, capture_output=True)
    decodificado = subprocess.run([programa, codificado, *eco,
                                   "--diagnostico", carpeta / "decodificacion.csv"],
                                  check=True, capture_output=True, text=True)
    recuperacion = ec.tasa_recuperacion(carpeta / "codificacion.csv",
                                        carpeta / "decodificacion.csv")
    return len(decodificado.stdout.splitlines()), recuperacion


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--resultados", type=Path, default=RAIZ / "resultados")
    parser.add_argument("--programa", type=Path, default=RAIZ / "bin" / "ecoesteg")
    args = parser.parse_args()
    if not all((ap.ORIGINALES / f"{c}.wav").exists() for c in CANCIONES):
        print("Faltan las canciones completas: corra make datos y ajuste_parametros.py.")
        return 1

    filas = []
    with tempfile.TemporaryDirectory() as tmp:
        for r0, r1, a in combinaciones(args.resultados / "ajuste_parametros.csv"):
            fila: dict = {"retardo0_us": r0, "retardo1_us": r1, "atenuacion": a}
            for c in CANCIONES:
                n, rec = medir(args.programa, sin_prueba(c, Path(tmp)), r0, r1, a, Path(tmp))
                fila[f"metadatos_{c}"] = n
                fila[f"recuperacion_{c}"] = round(rec, 2)
            fila["metadatos_total"] = sum(fila[f"metadatos_{c}"] for c in CANCIONES)
            fila["recuperacion_minima"] = min(fila[f"recuperacion_{c}"] for c in CANCIONES)
            filas.append(fila)
            print(f"{r0:4d}/{r1:4d} us, {a} %: {fila['metadatos_total']} de "
                  f"{len(SIETE) * len(CANCIONES)} metadatos, recuperación mínima "
                  f"{fila['recuperacion_minima']:.1f} %")
    ec.escribir_csv(args.resultados / "ajuste_canciones.csv", filas)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
