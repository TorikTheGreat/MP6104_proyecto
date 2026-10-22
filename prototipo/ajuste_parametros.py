"""Búsqueda de los valores por defecto de retardos y atenuación (R18, R32, R46).

Para no ajustarlos a los fragmentos de datos/ con los que se reportan los
resultados, la búsqueda usa otros tramos de 20 s: partes de las mismas grabaciones
que no se cruzan con esos fragmentos (datos/originales/, que descarga
make datos) y dos canciones con voz de dominio público de la banda Max Impact
(Fuerza Aérea de EE. UU.), que se descargan aquí. Para cada combinación de retardos
y atenuación mide, con bin/ecoesteg, el PESQ y la recuperación de bits promedio de
cada pieza; una combinación sirve si todas las piezas recuperan al menos 90 %.

Tarda unos 7 minutos. Escribe resultados/ajuste_parametros.csv.

    python prototipo/ajuste_parametros.py
"""

import argparse
import itertools
import math
import subprocess
import tempfile
import wave
from collections import defaultdict
from pathlib import Path

import experimentos_calidad as ec
import parametros as p

RAIZ = Path(__file__).resolve().parent.parent
ORIGINALES = RAIZ / "datos" / "originales"
MAX_IMPACT = {
    "change_my_mind": "https://upload.wikimedia.org/wikipedia/commons/e/ef/"
                      "Change_My_Mind_-_Max_Impact_-_United_States_Air_Force_Band.mp3",
    "what_i_want": "https://upload.wikimedia.org/wikipedia/commons/1/1c/"
                   "What_I_Want_-_Max_Impact_-_United_States_Air_Force_Band.mp3",
}
# Inicio (s) de cada tramo de 20 s; no se cruzan con los de datos/ (Vivaldi 85-105 s,
# Joplin y jazz 5-25 s).
TRAMOS = {
    "vivaldi_full": [30, 60, 120, 150, 180],
    "joplin_full": [30, 60, 90, 120, 150, 180],
    "jazz_full": [30, 60, 90, 120, 150, 180],
    "change_my_mind": [20, 60, 100, 140],
    "what_i_want": [20, 60, 100, 140],
}
RETARDOS = [(r0, r0 + dr) for r0 in (500, 750, 1000, 1250, 1500) for dr in (250, 500, 750)]
ATENUACIONES = [20, 25, 30]


def descargar_max_impact() -> None:
    for nombre, url in MAX_IMPACT.items():
        wav = ORIGINALES / f"{nombre}.wav"
        if wav.exists():
            continue
        mp3 = ORIGINALES / f"{nombre}.mp3"
        subprocess.run(["curl", "-f", "-s", "-L", "-A", "MP6104-TEC-Proyecto1/1.0", url,
                        "-o", mp3], check=True)
        subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", mp3, "-ar", "44100",
                        "-ac", "2", "-sample_fmt", "s16", wav], check=True)


def cortar_tramos(carpeta: Path) -> list[Path]:
    tramos = []
    for nombre, inicios in TRAMOS.items():
        with wave.open(str(ORIGINALES / f"{nombre}.wav"), "rb") as w:
            for s in inicios:
                if (s + 20) * p.FS > w.getnframes():
                    continue
                w.setpos(s * p.FS)
                destino = carpeta / f"{nombre}_{s:03d}.wav"
                with wave.open(str(destino), "wb") as o:
                    o.setnchannels(2)
                    o.setsampwidth(2)
                    o.setframerate(p.FS)
                    o.writeframes(w.readframes(20 * p.FS))
                tramos.append(destino)
    return tramos


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--resultados", type=Path, default=RAIZ / "resultados")
    parser.add_argument("--programa", type=Path, default=RAIZ / "bin" / "ecoesteg")
    args = parser.parse_args()
    if not all((ORIGINALES / f"{n}.wav").exists() for n in ("vivaldi_full", "joplin_full", "jazz_full")):
        print("Faltan las grabaciones originales: corra make datos.")
        return 1
    descargar_max_impact()

    filas = []
    with tempfile.TemporaryDirectory() as tmp:
        carpeta = Path(tmp)
        tramos = cortar_tramos(carpeta)
        for (r0, r1), a in itertools.product(RETARDOS, ATENUACIONES):
            pesq, recuperacion = defaultdict(list), defaultdict(list)
            for tramo in tramos:
                pieza = tramo.stem.rsplit("_", 1)[0]
                m = ec.medir(args.programa, tramo, (r0, r1), a, carpeta)
                if not math.isnan(m["pesq"]):
                    pesq[pieza].append(m["pesq"])
                recuperacion[pieza].append(m["recuperacion"])
            fila = {"retardo0_us": r0, "retardo1_us": r1, "atenuacion": a,
                    "tasa_bps": round(p.FS / p.largo_ventana(r0, r1), 2)}
            for pieza in TRAMOS:
                fila[f"pesq_{pieza}"] = round(sum(pesq[pieza]) / len(pesq[pieza]), 3)
                fila[f"recuperacion_{pieza}"] = round(
                    sum(recuperacion[pieza]) / len(recuperacion[pieza]), 2)
            fila["pesq_minimo"] = min(fila[f"pesq_{q}"] for q in TRAMOS)
            fila["recuperacion_minima"] = min(fila[f"recuperacion_{q}"] for q in TRAMOS)
            filas.append(fila)
            print(f"{r0:4d}/{r1:4d} us, {a} %: PESQ mínimo {fila['pesq_minimo']:.2f}, "
                  f"recuperación mínima {fila['recuperacion_minima']:.1f} %")
    ec.escribir_csv(args.resultados / "ajuste_parametros.csv", filas)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
