"""Calidad perceptual y recuperación con el programa final (R32, A28, A30, A31, A32).

Codifica cada fragmento de datos/ con bin/ecoesteg (el mensaje repetido en todas
las ventanas, como funciona el programa), lo decodifica y mide:
- la calidad: PESQ de banda ancha (ITU-T P.862.2) sobre las señales remuestreadas a
  16 kHz, y la relación señal a ruido del canal codificado;
- la recuperación: los bits que el decodificador lee igual que los que se
  codificaron, comparando los CSV de --diagnostico, y los metadatos recuperados.

Dos barridos: la atenuación con los retardos por defecto (A28, A30, A32, R32) y los
retardos, que cambian la tasa de transferencia, con la atenuación por defecto (A31).
Escribe resultados/calidad_*.csv y figuras/calidad_*.pdf y .png.
"""

import argparse
import csv
import subprocess
import tempfile
import wave
from pathlib import Path

import numpy as np
from scipy.signal import resample_poly

import decodificador as dec
import parametros as p

RAIZ = Path(__file__).resolve().parent.parent
ARCHIVOS = [
    ("clasica_vivaldi.wav", "Clásica"),
    ("ragtime_joplin.wav", "Ragtime"),
    ("jazz_park.wav", "Jazz"),
    ("electronica_sintetica.wav", "Electrónica (sintética)"),
]
METADATOS = ["--titulo", "Canción de prueba", "--artista", "Banda", "--anio", "2026"]
ATENUACIONES = [10, 20, 25, 30, 40, 50]
RETARDOS = [(500, 750), (750, 1000), (750, 1500), (1000, 1500), (1500, 2000)]
# Un color y un marcador por género, en orden fijo.
ESTILOS = [("#2a78d6", "o"), ("#eb6834", "s"), ("#1baf7a", "^"), ("#eda100", "D")]


def leer_izquierdo(ruta: Path) -> np.ndarray:
    with wave.open(str(ruta), "rb") as w:
        crudo = w.readframes(w.getnframes())
    return np.frombuffer(crudo, dtype="<i2").reshape(-1, 2)[:, 0].astype(np.float64)


def pesq_banda_ancha(referencia: np.ndarray, evaluada: np.ndarray) -> float:
    """NaN si PESQ no encuentra voz: su detector de actividad está hecho para voz y
    rechaza algunas señales, como la electrónica sintética."""
    from pesq import PesqError, pesq
    ref = resample_poly(referencia / 32768.0, 160, 441)
    deg = resample_poly(evaluada / 32768.0, 160, 441)
    valor = float(pesq(16000, ref, deg, "wb", on_error=PesqError.RETURN_VALUES))
    return valor if valor >= 0 else float("nan")


def snr_db(referencia: np.ndarray, evaluada: np.ndarray) -> float:
    ruido = np.sum((evaluada - referencia) ** 2)
    return float(10 * np.log10(np.sum(referencia ** 2) / ruido)) if ruido else float("inf")


def tasa_recuperacion(csv_codificacion: Path, csv_decodificacion: Path) -> float:
    """Porcentaje de las ventanas con bit en que el decodificador lee el mismo bit
    (el que pasa a la trama: el empate y r0 > r1 dan 0)."""
    with open(csv_codificacion, encoding="utf-8") as f:
        enviados = {int(r["ventana"]): int(r["bit"]) for r in csv.DictReader(f)}
    with open(csv_decodificacion, encoding="utf-8") as f:
        leidos = {int(r["ventana"]): 0 if dec.empate(float(r["r0"]), float(r["r1"]))
                  or float(r["r0"]) > float(r["r1"]) else 1 for r in csv.DictReader(f)}
    pares = [(b, leidos[k]) for k, b in enviados.items() if b >= 0 and k in leidos]
    return 100.0 * sum(a == b for a, b in pares) / len(pares)


def medir(programa: Path, original: Path, retardos: tuple[int, int], atenuacion: int,
          carpeta: Path) -> dict:
    eco = ["--retardo0", str(retardos[0]), "--retardo1", str(retardos[1])]
    codificado = carpeta / "codificado.wav"
    subprocess.run([programa, original, codificado, *METADATOS, *eco,
                    "--atenuacion0", str(atenuacion), "--atenuacion1", str(atenuacion),
                    "--diagnostico", carpeta / "codificacion.csv"],
                   check=True, capture_output=True)
    decodificado = subprocess.run([programa, codificado, *eco,
                                   "--diagnostico", carpeta / "decodificacion.csv"],
                                  check=True, capture_output=True, text=True)
    x = leer_izquierdo(original)
    y = leer_izquierdo(codificado)[:len(x)]
    return {
        "pesq": round(pesq_banda_ancha(x, y), 3),
        "snr_db": round(snr_db(x, y), 2),
        "recuperacion": round(tasa_recuperacion(carpeta / "codificacion.csv",
                                                carpeta / "decodificacion.csv"), 2),
        "metadatos": len(decodificado.stdout.splitlines()),
    }


def escribir_csv(ruta: Path, filas: list[dict]) -> None:
    with open(ruta, "w", newline="", encoding="utf-8") as f:
        escritor = csv.DictWriter(f, fieldnames=list(filas[0]))
        escritor.writeheader()
        escritor.writerows(filas)
    print(f"Tabla guardada: {ruta.relative_to(RAIZ) if ruta.is_relative_to(RAIZ) else ruta}")


def correlacion(filas: list[dict], x: str) -> tuple[int, float]:
    """Pearson entre x y PESQ, sin los puntos donde PESQ no dio valor."""
    puntos = [(f[x], f["pesq"]) for f in filas if not np.isnan(f["pesq"])]
    xs, ys = zip(*puntos)
    return len(puntos), round(float(np.corrcoef(xs, ys)[0, 1]), 3)


def figura(nombre: str, filas: list[dict], x: str, etiqueta_x: str, carpeta: Path,
           orden: str | None = None) -> None:
    """PESQ frente a x, una línea por género; los puntos se unen en el orden de la
    columna orden (por defecto, x)."""
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    fig, ax = plt.subplots(figsize=(3.5, 2.5))
    for (archivo, genero), (color, marcador) in zip(ARCHIVOS, ESTILOS):
        puntos = [(f[x], f["pesq"]) for f in sorted(filas, key=lambda f: f[orden or x])
                  if f["archivo"] == archivo and not np.isnan(f["pesq"])]
        if not puntos:
            continue
        ax.plot(*zip(*puntos), color=color, marker=marcador, markersize=5, linewidth=1.5,
                label=genero)
    ax.set_xlabel(etiqueta_x, fontsize=8)
    ax.set_ylabel("PESQ (banda ancha)", fontsize=8)
    ax.tick_params(labelsize=7)
    ax.grid(True, alpha=0.3, linewidth=0.5)
    ax.legend(fontsize=6.5, frameon=False)
    fig.tight_layout()
    for ext in ("pdf", "png"):
        fig.savefig(carpeta / f"{nombre}.{ext}", dpi=200, metadata={"CreationDate": None}
                    if ext == "pdf" else None)
    plt.close(fig)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--datos", type=Path, default=RAIZ / "datos")
    parser.add_argument("--resultados", type=Path, default=RAIZ / "resultados")
    parser.add_argument("--figuras", type=Path, default=RAIZ / "figuras")
    parser.add_argument("--programa", type=Path, default=RAIZ / "bin" / "ecoesteg")
    args = parser.parse_args()
    if not args.programa.exists():
        print(f"No está {args.programa}: compile con make.")
        return 1
    if not all((args.datos / a).exists() for a, _ in ARCHIVOS):
        print("Faltan los WAV de datos/: corra make datos.")
        return 1

    por_atenuacion, por_retardos = [], []
    with tempfile.TemporaryDirectory() as tmp:
        carpeta = Path(tmp)
        for archivo, genero in ARCHIVOS:
            original = args.datos / archivo
            for a in ATENUACIONES:
                m = medir(args.programa, original, (p.RETARDO0_US, p.RETARDO1_US), a, carpeta)
                por_atenuacion.append({"archivo": archivo, "genero": genero, "atenuacion": a, **m})
                print(f"{genero:24s} a={a:2d} %  PESQ {m['pesq']:.2f}  SNR {m['snr_db']:5.1f} dB  "
                      f"recuperación {m['recuperacion']:6.2f} %  metadatos {m['metadatos']}/3")
            for r0, r1 in RETARDOS:
                m = medir(args.programa, original, (r0, r1), int(p.ATENUACION0), carpeta)
                tasa = round(p.FS / p.largo_ventana(r0, r1), 2)
                por_retardos.append({"archivo": archivo, "genero": genero, "retardo0_us": r0,
                                     "retardo1_us": r1, "tasa_bps": tasa, **m})

    args.resultados.mkdir(parents=True, exist_ok=True)
    args.figuras.mkdir(parents=True, exist_ok=True)
    escribir_csv(args.resultados / "calidad_atenuacion.csv", por_atenuacion)
    escribir_csv(args.resultados / "calidad_retardos.csv", por_retardos)
    n30, r30 = correlacion(por_atenuacion, "recuperacion")
    n31, r31 = correlacion(por_retardos, "tasa_bps")
    escribir_csv(args.resultados / "calidad_correlaciones.csv", [
        {"relacion": "PESQ y recuperación, barrido de atenuación (A30)", "puntos": n30,
         "pearson": r30},
        {"relacion": "PESQ y tasa de transferencia, barrido de retardos (A31)", "puntos": n31,
         "pearson": r31},
    ])
    figura("calidad_vs_atenuacion", por_atenuacion, "atenuacion", "Atenuación del eco (%)",
           args.figuras)
    figura("calidad_vs_recuperacion", por_atenuacion, "recuperacion",
           "Recuperación de bits (%)", args.figuras, orden="atenuacion")
    figura("calidad_vs_tasa", por_retardos, "tasa_bps", "Tasa de transferencia (bps)",
           args.figuras)
    print(f"Figuras guardadas en: {args.figuras}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
