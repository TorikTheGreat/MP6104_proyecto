
from pathlib import Path
import csv

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from scipy.io import wavfile
from scipy.signal import correlate

RAIZ = Path(__file__).resolve().parents[1]
SALIDA = RAIZ / "resultados" / "modulo_a"
SALIDA.mkdir(parents=True, exist_ok=True)

fs, original = wavfile.read(RAIZ / "datos/vivaldi_10s.wav")
fs2, codificado = wavfile.read(
    RAIZ / "datos/vivaldi_codificado_cpp_nuevo.wav"
)

if fs != fs2:
    raise ValueError("Las frecuencias de muestreo no coinciden")

x = original[:, 0].astype(np.float64)
y = codificado[:, 0].astype(np.float64)

bits = "101101"
d0 = round(1000 * fs / 1_000_000)
d1 = round(2000 * fs / 1_000_000)
N = 25 * max(d0, d1)

filas = []

fig, axes = plt.subplots(2, 3, figsize=(13, 7))

for i, bit in enumerate(bits):
    inicio = i * N
    fin = inicio + N

    # Diferencia entre audio codificado y original.
    # Incluye contribuciones de ecos de ventanas anteriores.
    residuo = y[inicio:fin] - x[inicio:fin]

    # Correlación cruzada entre la ventana original y el residuo.
    referencia = x[inicio:fin]
    corr = correlate(residuo, referencia, mode="full", method="fft")
    retardos = np.arange(-N + 1, N)

    # Examinar únicamente retardos positivos cercanos a los esperados.
    mascara = (retardos >= 1) & (retardos <= 150)
    retardos_validos = retardos[mascara]
    corr_valida = corr[mascara]

    detectado = int(retardos_validos[np.argmax(np.abs(corr_valida))])
    esperado = d0 if bit == "0" else d1

    filas.append({
        "ventana": i + 1,
        "bit": bit,
        "retardo_esperado": esperado,
        "retardo_detectado": detectado,
        "coincide": detectado == esperado,
    })

    ax = axes.flat[i]
    ax.plot(retardos_validos, corr_valida)
    ax.axvline(esperado, color="red", linestyle="--",
               label=f"Esperado: {esperado}")
    ax.set_title(f"Ventana {i+1} - bit {bit}")
    ax.set_xlabel("Retardo (muestras)")
    ax.set_ylabel("Correlación")
    ax.legend()
    ax.grid(alpha=0.3)

    print(
        f"Ventana {i+1}: bit={bit}, "
        f"esperado={esperado}, detectado={detectado}, "
        f"coincide={detectado == esperado}"
    )

fig.tight_layout()
fig.savefig(SALIDA / "correlaciones.png", dpi=180)
plt.close(fig)

with (SALIDA / "correlaciones.csv").open(
    "w", newline="", encoding="utf-8"
) as archivo:
    escritor = csv.DictWriter(archivo, fieldnames=filas[0].keys())
    escritor.writeheader()
    escritor.writerows(filas)

aciertos = sum(fila["coincide"] for fila in filas)
print(f"\nRetardos correctos: {aciertos}/{len(bits)}")
print("Gráfica: resultados/modulo_a/correlaciones.png")
print("CSV: resultados/modulo_a/correlaciones.csv")
