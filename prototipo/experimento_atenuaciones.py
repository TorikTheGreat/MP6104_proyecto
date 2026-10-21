#!/usr/bin/env python3
"""Barrido de atenuaciones para el codificador de eco (módulo A).

Ejecutar desde la raíz del repositorio:
  python prototipo/experimento_atenuaciones.py
"""
from pathlib import Path
import csv
import math
import sys

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np
from scipy.io import wavfile

RAIZ = Path(__file__).resolve().parents[1]
ORIGINAL = RAIZ / 'datos' / 'vivaldi_10s.wav'
REFERENCIA_CPP = RAIZ / 'datos' / 'vivaldi_codificado_cpp_nuevo.wav'
RESULTADOS = RAIZ / 'resultados' / 'modulo_a'
BITS = '101101'
TAU0_US = 1000
TAU1_US = 2000
K = 25
ATENUACIONES = (0.05, 0.10, 0.15, 0.20, 0.25)


def codificar(x, bits, d0, d1, a, ventana):
    """Replica la suma de ecos del prototipo, sin cuantizar a PCM16."""
    y = np.zeros(len(x) + max(d0, d1), dtype=np.float64)
    y[:len(x)] = x
    for indice, bit in enumerate(bits):
        inicio = indice * ventana
        fin = min(inicio + ventana, len(x))
        if inicio >= len(x):
            break
        d = d0 if bit == '0' else d1
        y[inicio + d:fin + d] += a * x[inicio:fin]
    return y


def pcm16(y):
    # Igual que C++: saturación y truncamiento hacia cero.
    return np.clip(y, -32768, 32767).astype(np.int16)


def main():
    if not ORIGINAL.exists():
        sys.exit(f'No se encontró {ORIGINAL}')
    fs, audio = wavfile.read(ORIGINAL)
    if audio.ndim != 2 or audio.shape[1] != 2 or audio.dtype != np.int16:
        sys.exit('Se requiere WAV estéreo PCM16.')
    d0 = round(TAU0_US * fs / 1_000_000)
    d1 = round(TAU1_US * fs / 1_000_000)
    ventana = K * max(d0, d1)
    x = audio[:, 0].astype(np.float64)
    potencia = np.mean((x / 32768.0) ** 2)
    RESULTADOS.mkdir(parents=True, exist_ok=True)
    filas = []
    for a in ATENUACIONES:
        y = pcm16(codificar(x, BITS, d0, d1, a, ventana))
        diferencia = (x - y[:len(x)].astype(np.float64)) / 32768.0
        mse = float(np.mean(diferencia ** 2))
        snr = float(10 * math.log10(potencia / mse)) if mse else math.inf
        filas.append({'atenuacion': a, 'mse': mse, 'snr_db': snr,
                      'muestras_modificadas': int(np.count_nonzero(diferencia))})
        print(f'a={a:.2f}  MSE={mse:.10g}  SNR={snr:.3f} dB  '
              f'muestras distintas={filas[-1]["muestras_modificadas"]}')
        if abs(a - 0.20) < 1e-10 and REFERENCIA_CPP.exists():
            fs_cpp, cpp = wavfile.read(REFERENCIA_CPP)
            if fs_cpp != fs or cpp.ndim != 2 or cpp.shape[1] != 2:
                sys.exit('Referencia C++ incompatible.')
            if len(cpp) != len(y):
                sys.exit('ALERTA: longitud Python distinta de C++. Revisar parámetros.')
            discrepancias = np.count_nonzero(cpp[:, 0] != y)
            print(f'Comparación a=0.20 con C++: {discrepancias} muestras PCM distintas')
            if discrepancias > 1:
                sys.exit('ALERTA: Python y C++ no coinciden. Revisar parámetros y codificación.')
            if not np.array_equal(cpp[:len(x), 1], audio[:, 1]):
                sys.exit('ALERTA: canal derecho C++ distinto del original.')
    csv_path = RESULTADOS / 'atenuaciones.csv'
    with csv_path.open('w', newline='', encoding='utf-8') as f:
        writer = csv.DictWriter(f, fieldnames=list(filas[0]))
        writer.writeheader()
        writer.writerows(filas)
    fig, ax = plt.subplots(figsize=(7, 4))
    ax.plot([r['atenuacion'] for r in filas], [r['snr_db'] for r in filas], marker='o')
    ax.set(xlabel='Atenuación del eco', ylabel='SNR (dB)',
           title='Calidad de audio frente a atenuación del eco')
    ax.grid(True, alpha=0.3)
    fig.tight_layout()
    png_path = RESULTADOS / 'snr_vs_atenuacion.png'
    fig.savefig(png_path, dpi=180)
    plt.close(fig)
    print(f'\nCSV: {csv_path.relative_to(RAIZ)}')
    print(f'Gráfica: {png_path.relative_to(RAIZ)}')
    print('Nota: no se evalúa recuperación de bits; requiere el decodificador.')


if __name__ == '__main__':
    main()
