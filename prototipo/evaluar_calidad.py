#!/usr/bin/env python3
"""Evalúa la distorsión introducida por el codificador de eco en archivos WAV."""
import argparse
import math
from math import gcd

import numpy as np
from scipy.io import wavfile
from scipy.signal import resample_poly


def metricas(referencia, evaluada):
    referencia = np.asarray(referencia, dtype=np.float64)
    evaluada = np.asarray(evaluada, dtype=np.float64)
    error = referencia - evaluada
    mse = float(np.mean(error ** 2))
    potencia = float(np.mean(referencia ** 2))
    snr = math.inf if mse == 0 else (10 * math.log10(potencia / mse) if potencia > 0 else -math.inf)
    return mse, snr


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('original', help='WAV de referencia')
    parser.add_argument('codificado', help='WAV con eco')
    parser.add_argument('--canal', choices=['izquierdo', 'derecho'], default='izquierdo')
    args = parser.parse_args()

    fs_original, original = wavfile.read(args.original)
    fs_codificado, codificado = wavfile.read(args.codificado)
    if fs_original != fs_codificado:
        parser.error('Los archivos deben tener la misma frecuencia de muestreo.')
    if original.ndim != 2 or codificado.ndim != 2 or original.shape[1] != 2 or codificado.shape[1] != 2:
        parser.error('Se esperan dos WAV estéreo.')
    if original.dtype != np.int16 or codificado.dtype != np.int16:
        parser.error('Se esperan archivos WAV PCM de 16 bits.')
    if codificado.shape[0] < original.shape[0]:
        parser.error('El audio codificado es más corto que el original.')

    n = original.shape[0]
    indice = 0 if args.canal == 'izquierdo' else 1
    ref = original[:, indice].astype(np.float64) / 32768.0
    mod = codificado[:n, indice].astype(np.float64) / 32768.0
    mse, snr = metricas(ref, mod)
    print(f'Frecuencia de muestreo: {fs_original} Hz')
    print(f'Muestras originales: {n}')
    print(f'Muestras adicionales del codificado: {codificado.shape[0] - n}')
    print(f'Canal evaluado: {args.canal}')
    print(f'MSE (audio normalizado): {mse:.10g}')
    print(f'SNR: {snr:.3f} dB')
    print(f'Muestras distintas: {np.count_nonzero(original[:, indice] != codificado[:n, indice])} de {n}')
    otro = 1 - indice
    iguales = np.array_equal(original[:, otro], codificado[:n, otro])
    print(f'Canal no evaluado idéntico (primeras {n} muestras): {iguales}')

    try:
        from pesq import pesq
        objetivo = 16000
        divisor = gcd(fs_original, objetivo)
        ref16 = resample_poly(ref, objetivo // divisor, fs_original // divisor)
        mod16 = resample_poly(mod, objetivo // divisor, fs_original // divisor)
        resultado = pesq(objetivo, ref16, mod16, 'wb')
        print(f'PESQ-WB (16 kHz): {resultado:.4f}')
        print('Advertencia: PESQ se diseñó para voz; en música es solo una referencia exploratoria.')
    except Exception as exc:
        print(f'PESQ no se pudo calcular: {type(exc).__name__}: {exc}')


if __name__ == '__main__':
    main()
