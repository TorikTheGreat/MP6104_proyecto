#!/usr/bin/env python3
# Dueño: B.
"""Experimentos de tasa de recuperación, barrido de parámetros y archivos sin codificar (A29, A32, A33, R18, R46).

Uso:
    python3 prototipo/experimentos_recuperacion.py --datos datos --resultados resultados --programa bin/ecoesteg
"""

import argparse
import csv
import sys
import wave
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

RAIZ = Path(__file__).resolve().parent.parent
if str(RAIZ / "prototipo") not in sys.path:
    sys.path.insert(0, str(RAIZ / "prototipo"))

import decodificador as dec  # noqa: E402
import metadatos  # noqa: E402
import parametros as p  # noqa: E402

SEMILLA = 44100

PIEZAS = (
    ("clasica_vivaldi.wav", "Clásica (cuerdas)"),
    ("ragtime_joplin.wav", "Ragtime (piano)"),
    ("jazz_park.wav", "Jazz"),
    ("electronica_sintetica.wav", "Electrónica"),
)

ATENUACIONES_PORCENTAJE = (10.0, 20.0, 25.0, 30.0, 40.0, 50.0)
FACTORES_K = (10, 15, 20, 25, 30)
PARES_RETARDOS_US = (
    (500, 750),
    (750, 1000),
    (750, 1500),
    (1000, 1500),
    (1500, 2000),
)


def leer_canal_izquierdo(ruta_wav: Path) -> np.ndarray:
    with wave.open(str(ruta_wav), "rb") as w:
        if w.getnchannels() != 2 or w.getsampwidth() != 2 or w.getframerate() != p.FS:
            raise ValueError(f"Formato WAV incompatible en {ruta_wav}")
        raw = w.readframes(w.getnframes())
    estereo = np.frombuffer(raw, dtype=np.int16).reshape(-1, 2)
    return estereo[:, 0]


def codificar_solapamiento_y_suma(
    x_int16: np.ndarray, bits: list[int], d0: int, d1: int, a0: float, a1: float, largo: int
) -> np.ndarray:
    """Codifica bits en el canal mediante convolución por ventana con solapamiento y suma."""
    x = x_int16.astype(np.float64)
    y = x.copy()
    for k, b in enumerate(bits):
        d = d0 if b == 0 else d1
        a = a0 if b == 0 else a1
        ini = k * largo
        fin = ini + largo
        if fin > x.size:
            break
        dest_fin = min(y.size, fin + d)
        n_eco = dest_fin - (ini + d)
        if n_eco > 0:
            y[ini + d : dest_fin] += a * x[ini : ini + n_eco]
    return np.clip(y, -32768, 32767).astype(np.int16)


def medir_tasa_recuperacion(
    x_int16: np.ndarray,
    d0: int,
    d1: int,
    a0: float,
    a1: float,
    largo: int,
    rng: np.random.Generator,
) -> tuple[int, int, float]:
    n_ventanas = int(x_int16.size // largo)
    bits_ref = rng.integers(0, 2, size=n_ventanas).tolist()
    y_int16 = codificar_solapamiento_y_suma(x_int16, bits_ref, d0, d1, a0, a1, largo)
    res = dec.decodificar_senal(y_int16, d0, d1, largo_ventana=largo)
    # Evaluamos tasa por bit sobre las ventanas clasificadas y también sobre el vector decodificado
    aciertos = 0
    for k, b_esp in enumerate(bits_ref):
        v = res.ventanas[k]
        b_obt = 0 if v.r0 >= v.r1 else 1
        if b_obt == b_esp:
            aciertos += 1
    tasa = 100.0 * aciertos / n_ventanas if n_ventanas > 0 else 0.0
    return aciertos, n_ventanas, tasa


def escribir_csv(ruta: Path, filas: list[dict[str, object]]) -> None:
    if not filas:
        return
    with ruta.open("w", newline="", encoding="utf-8") as f:
        escritor = csv.DictWriter(f, fieldnames=list(filas[0].keys()))
        escritor.writeheader()
        escritor.writerows(filas)
    print(f"Tabla guardada: {ruta.resolve().relative_to(RAIZ)}")


def generar_figuras(
    dir_figuras: Path,
    filas_aten: list[dict[str, object]],
    muestra_senal: np.ndarray,
) -> None:
    dir_figuras.mkdir(parents=True, exist_ok=True)

    # 1. Figura: Tasa de recuperación frente a atenuación por género
    fig, ax = plt.subplots(figsize=(5.2, 3.3))
    marcadores = ("o", "s", "^", "D")
    for idx, (archivo, genero) in enumerate(PIEZAS):
        sub = [f for f in filas_aten if f["archivo"] == archivo]
        xs = [float(f["atenuacion_porcentaje"]) for f in sub]
        ys = [float(f["tasa_recuperacion_porcentaje"]) for f in sub]
        ax.plot(xs, ys, marker=marcadores[idx % len(marcadores)], linewidth=1.4, label=genero)

    sub_prom = [f for f in filas_aten if f["archivo"] == "PROMEDIO"]
    if sub_prom:
        xs_p = [float(f["atenuacion_porcentaje"]) for f in sub_prom]
        ys_p = [float(f["tasa_recuperacion_porcentaje"]) for f in sub_prom]
        ax.plot(xs_p, ys_p, color="black", linestyle="--", linewidth=2.0, marker="*", label="Promedio global")

    ax.axhline(90.0, color="crimson", linestyle=":", linewidth=1.2, label="Meta mínima R46 (90 %)")
    ax.set_xlabel("Atenuación del eco (%)")
    ax.set_ylabel("Tasa de recuperación (%)")
    ax.set_ylim(50.0, 103.0)
    ax.grid(True, alpha=0.3)
    ax.legend(fontsize=8, loc="lower right")
    fig.tight_layout()
    for ext in ("pdf", "png"):
        fig.savefig(dir_figuras / f"recuperacion_vs_atenuacion.{ext}", dpi=200)
    plt.close(fig)

    # 2. Figura: Autocorrelación del cepstro para bit 0, bit 1 y ventana sin eco
    d0 = p.muestras_desde_us(p.RETARDO0_US)
    d1 = p.muestras_desde_us(p.RETARDO1_US)
    largo = p.largo_ventana(p.RETARDO0_US, p.RETARDO1_US)
    v_orig = muestra_senal[:largo].astype(np.float64)
    v_bit0 = v_orig.copy()
    a_def = p.amplitud_desde_porcentaje(p.ATENUACION0)
    v_bit0[d0:] += a_def * v_orig[:-d0]
    v_bit1 = v_orig.copy()
    v_bit1[d1:] += a_def * v_orig[:-d1]

    rcc_orig = dec.autocorrelacion_cepstro(v_orig)
    rcc_bit0 = dec.autocorrelacion_cepstro(v_bit0)
    rcc_bit1 = dec.autocorrelacion_cepstro(v_bit1)

    lags = np.arange(15, 100)
    fig, axes = plt.subplots(3, 1, figsize=(5.2, 4.4), sharex=True)
    casos = (
        (axes[0], rcc_bit0, f"a) Ventana con bit 0 codificado ($d_0 = {d0}$ muestras)"),
        (axes[1], rcc_bit1, f"b) Ventana con bit 1 codificado ($d_1 = {d1}$ muestras)"),
        (axes[2], rcc_orig, "c) Ventana sin codificar (sin eco)"),
    )
    for ax_i, rcc_i, titulo in casos:
        ax_i.plot(lags, rcc_i[lags], color="#1f4e78", linewidth=1.2)
        ax_i.axvline(d0, color="#2e8b57", linestyle="--", linewidth=1.0, label=f"$d_0={d0}$")
        ax_i.axvline(d1, color="#d95f02", linestyle="-.", linewidth=1.0, label=f"$d_1={d1}$")
        ax_i.axhline(dec.UMBRAL_ECO, color="crimson", linestyle=":", linewidth=1.0, label="Umbral eco")
        ax_i.set_title(titulo, fontsize=8.5)
        ax_i.set_ylabel("$R_{cc}(n)$", fontsize=8)
        ax_i.axhline(0.0, color="black", linewidth=0.5)
        ax_i.set_ylim(min(-1.0, float(np.min(rcc_orig[lags])) * 1.12),
                      max(3.5, float(np.max(rcc_bit0[lags])) * 1.12))
        ax_i.grid(True, alpha=0.3)
    axes[0].legend(fontsize=7.5, loc="upper right", ncol=3)
    axes[2].set_xlabel("Quefrencia $n$ (muestras)")
    fig.tight_layout()
    for ext in ("pdf", "png"):
        fig.savefig(dir_figuras / f"cepstro_con_y_sin_eco.{ext}", dpi=200)
    plt.close(fig)

    # 3. Figura: Diagrama de bloques del subsistema decodificador (A12, A20)
    from matplotlib.patches import FancyArrowPatch, FancyBboxPatch

    fig, ax = plt.subplots(figsize=(6.4, 2.2))
    ax.set_xlim(0, 10.2)
    ax.set_ylim(0, 3.0)
    ax.axis("off")

    bloques = [
        (0.2, 1.55, 1.65, 1.0, "Enventanado\n$v_k(n)$\n$L = K \\max(d_0, d_1)$"),
        (2.25, 1.55, 1.75, 1.0, "Normalización\npor valor eficaz\n$\\hat{v}_k = v_k \\frac{\\mathrm{RMS}_{\\mathrm{ref}}}{\\mathrm{RMS}(v_k)}$"),
        (4.4, 1.55, 1.75, 1.0, "Dominio espectral\n$X_k = \\mathrm{FFT}(\\hat{v}_k)$\n$S_k = (\\ln|X_k|)^2$"),
        (6.55, 1.55, 1.55, 1.0, "Autocorrelación\ndel cepstro\n$R_{cc} = \\mathrm{Re}\\{\\mathrm{IFFT}(S_k)\\}$"),
        (8.45, 1.55, 1.55, 1.0, "Clasificador\n$r_0=R_{cc}(d_0)$\n$r_1=R_{cc}(d_1)$"),
        (4.4, 0.15, 2.8, 0.85, "Filtrado de colas y señales\nsin eco ($\\mathrm{SIN\\_ECO} = -1$)"),
        (7.7, 0.15, 2.3, 0.85, "Secuencia de bits\n$\\rightarrow$ Trama de metadatos"),
    ]
    for bx, by, bw, bh, texto in bloques:
        caja = FancyBboxPatch(
            (bx, by),
            bw,
            bh,
            boxstyle="round,pad=0.06,rounding_size=0.1",
            edgecolor="#1f4e78",
            facecolor="#edf4fa",
            linewidth=1.2,
        )
        ax.add_patch(caja)
        ax.text(bx + bw / 2.0, by + bh / 2.0, texto, ha="center", va="center", fontsize=7.3)

    flechas = [
        ((1.85, 2.05), (2.25, 2.05)),
        ((4.0, 2.05), (4.4, 2.05)),
        ((6.15, 2.05), (6.55, 2.05)),
        ((8.1, 2.05), (8.45, 2.05)),
        ((9.22, 1.55), (5.8, 1.02)),
        ((7.2, 0.58), (7.7, 0.58)),
    ]
    for p_ini, p_fin in flechas:
        ax.add_patch(
            FancyArrowPatch(
                p_ini,
                p_fin,
                arrowstyle="-|>",
                mutation_scale=10,
                color="#333333",
                linewidth=1.1,
            )
        )
    fig.tight_layout(pad=0.1)
    for ext in ("pdf", "png"):
        fig.savefig(dir_figuras / f"diagrama_decodificador.{ext}", dpi=200)
    plt.close(fig)
    print(f"Figuras guardadas en: {dir_figuras.resolve().relative_to(RAIZ)}/")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--datos", type=Path, default=RAIZ / "datos")
    parser.add_argument("--resultados", type=Path, default=RAIZ / "resultados")
    parser.add_argument("--figuras", type=Path, default=RAIZ / "figuras")
    parser.add_argument("--programa", type=Path, default=RAIZ / "bin" / "ecoesteg")
    args = parser.parse_args()

    args.resultados.mkdir(parents=True, exist_ok=True)
    senales: dict[str, tuple[str, np.ndarray]] = {}
    for archivo, genero in PIEZAS:
        ruta = args.datos / archivo
        if not ruta.exists():
            print(f"Aviso: no existe {ruta}; ejecute 'make datos' primero.")
            return 0
        senales[archivo] = (genero, leer_canal_izquierdo(ruta))

    d0_def = p.muestras_desde_us(p.RETARDO0_US)
    d1_def = p.muestras_desde_us(p.RETARDO1_US)
    largo_def = p.largo_ventana(p.RETARDO0_US, p.RETARDO1_US)

    # 1. Barrido de atenuaciones por género musical (A29, A32)
    filas_aten: list[dict[str, object]] = []
    for aten_pct in ATENUACIONES_PORCENTAJE:
        a = p.amplitud_desde_porcentaje(aten_pct)
        tasas_genero: list[float] = []
        aciertos_tot = 0
        ventanas_tot = 0
        for idx, (archivo, genero) in enumerate(PIEZAS):
            rng = np.random.default_rng(SEMILLA + idx)
            _, x = senales[archivo]
            aciertos, n_vent, tasa = medir_tasa_recuperacion(x, d0_def, d1_def, a, a, largo_def, rng)
            tasas_genero.append(tasa)
            aciertos_tot += aciertos
            ventanas_tot += n_vent
            filas_aten.append({
                "archivo": archivo,
                "genero": genero,
                "retardo0_us": p.RETARDO0_US,
                "retardo1_us": p.RETARDO1_US,
                "k_ventana": p.K_VENTANA,
                "atenuacion_porcentaje": aten_pct,
                "aciertos": aciertos,
                "ventanas": n_vent,
                "tasa_recuperacion_porcentaje": round(tasa, 2),
            })
        tasa_media = float(np.mean(tasas_genero))
        filas_aten.append({
            "archivo": "PROMEDIO",
            "genero": "Promedio global",
            "retardo0_us": p.RETARDO0_US,
            "retardo1_us": p.RETARDO1_US,
            "k_ventana": p.K_VENTANA,
            "atenuacion_porcentaje": aten_pct,
            "aciertos": aciertos_tot,
            "ventanas": ventanas_tot,
            "tasa_recuperacion_porcentaje": round(tasa_media, 2),
        })
        print(f"[Atenuación {aten_pct:4.1f} %] Tasa media de recuperación (4 géneros): {tasa_media:.2f} %")

    escribir_csv(args.resultados / "recuperacion_atenuacion_generos.csv", filas_aten)

    # 2. Barrido de factor de ventana K y pares de retardos con la atenuación por defecto (R18, R33, R42)
    a_def = p.amplitud_desde_porcentaje(p.ATENUACION0)
    filas_kr: list[dict[str, object]] = []
    for t0_us, t1_us in PARES_RETARDOS_US:
        d0 = p.muestras_desde_us(t0_us)
        d1 = p.muestras_desde_us(t1_us)
        for k_val in FACTORES_K:
            largo = k_val * max(d0, d1)
            bps = p.FS / largo
            tasas_k: list[float] = []
            for idx, (archivo, _) in enumerate(PIEZAS):
                rng = np.random.default_rng(SEMILLA + 100 + idx)
                _, x = senales[archivo]
                _, _, tasa = medir_tasa_recuperacion(x, d0, d1, a_def, a_def, largo, rng)
                tasas_k.append(tasa)
            filas_kr.append({
                "retardo0_us": t0_us,
                "retardo1_us": t1_us,
                "d0_muestras": d0,
                "d1_muestras": d1,
                "k_ventana": k_val,
                "largo_ventana": largo,
                "tasa_transferencia_bps": round(bps, 2),
                "tasa_clasica": round(tasas_k[0], 2),
                "tasa_ragtime": round(tasas_k[1], 2),
                "tasa_jazz": round(tasas_k[2], 2),
                "tasa_electronica": round(tasas_k[3], 2),
                "tasa_promedio_porcentaje": round(float(np.mean(tasas_k)), 2),
            })
    escribir_csv(args.resultados / "recuperacion_ventana_retardos.csv", filas_kr)

    # 3. Evaluación sobre archivos sin codificar (A33, R47)
    filas_sin_eco: list[dict[str, object]] = []
    for archivo, genero in PIEZAS:
        _, x = senales[archivo]
        res_orig = dec.decodificar_senal(x, d0_def, d1_def, largo_ventana=largo_def)
        n_vent = len(res_orig.ventanas)
        n_sin_eco = sum(1 for v in res_orig.ventanas if v.bit == dec.SIN_ECO)
        picos = [max(v.r0, v.r1) for v in res_orig.ventanas]
        lecturas_falsas = metadatos.desde_bits(res_orig.bits)
        filas_sin_eco.append({
            "archivo": archivo,
            "genero": genero,
            "ventanas_totales": n_vent,
            "ventanas_sin_eco": n_sin_eco,
            "porcentaje_sin_eco": round(100.0 * n_sin_eco / max(n_vent, 1), 2),
            "pico_medio_rcc": round(float(np.mean(picos)), 3),
            "pico_p95_rcc": round(float(np.percentile(picos, 95)), 3),
            "bits_emitidos": len(res_orig.bits),
            "metadatos_falsos": len(lecturas_falsas),
        })
        print(
            f"[Sin codificar] {genero:20s}: {n_sin_eco}/{n_vent} ventanas sin eco "
            f"({100.0*n_sin_eco/n_vent:.1f} %), bits emitidos={len(res_orig.bits)}, "
            f"metadatos falsos={len(lecturas_falsas)}"
        )

    escribir_csv(args.resultados / "recuperacion_sin_codificar.csv", filas_sin_eco)

    generar_figuras(args.figuras, filas_aten, senales["electronica_sintetica.wav"][1])
    return 0


if __name__ == "__main__":
    sys.exit(main())
