#!/usr/bin/env bash
# Dueño: B.
# Descarga los archivos de audio originales en datos/originales/ (no versionada)
# y genera en datos/ los fragmentos WAV de prueba en formato CD (PCM 16 bits,
# 44.1 kHz, estéreo, Little Endian) de distintos géneros musicales (R25, A32).

set -euo pipefail

RAIZ="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ORIG="${RAIZ}/datos/originales"
DATOS="${RAIZ}/datos"
mkdir -p "${ORIG}" "${DATOS}"

UA="Mozilla/5.0 (X11; Linux x86_64) MP6104-TEC-Proyecto1/1.0"

descargar() {
    local url="$1"
    local destino="$2"
    if [ ! -s "${destino}" ]; then
        echo "Descargando $(basename "${destino}")..."
        curl -f -s -L --retry 2 -A "${UA}" "${url}" -o "${destino}" || return 1
    fi
    return 0
}

convertir_a_wav_completo() {
    local entrada="$1"
    local salida="$2"
    if [ -s "${salida}" ]; then
        return 0
    fi
    if command -v ffmpeg >/dev/null 2>&1; then
        ffmpeg -y -loglevel error -i "${entrada}" -ar 44100 -ac 2 -sample_fmt s16 "${salida}"
    elif command -v gst-launch-1.0 >/dev/null 2>&1; then
        gst-launch-1.0 -q filesrc location="${entrada}" ! decodebin ! audioconvert ! audioresample ! \
            "audio/x-raw,format=S16LE,channels=2,rate=44100" ! wavenc ! filesink location="${salida}" >/dev/null 2>&1
    else
        return 1
    fi
}

VIVALDI_URL="https://upload.wikimedia.org/wikipedia/commons/f/ff/Vivaldi_-_Four_Seasons_1_Spring_mvt_1_Allegro_-_John_Harrison_violin.oga"
JOPLIN_URL="https://upload.wikimedia.org/wikipedia/commons/6/62/Scott_Joplin_-_The_Entertainer_%281902%29.ogg"
JAZZ_URL="https://upload.wikimedia.org/wikipedia/commons/0/03/Jazz_at_the_park.ogg"

if descargar "${VIVALDI_URL}" "${ORIG}/vivaldi.oga"; then
    convertir_a_wav_completo "${ORIG}/vivaldi.oga" "${ORIG}/vivaldi_full.wav" || true
fi
if descargar "${JOPLIN_URL}" "${ORIG}/joplin.ogg"; then
    convertir_a_wav_completo "${ORIG}/joplin.ogg" "${ORIG}/joplin_full.wav" || true
fi
if descargar "${JAZZ_URL}" "${ORIG}/jazz.ogg"; then
    convertir_a_wav_completo "${ORIG}/jazz.ogg" "${ORIG}/jazz_full.wav" || true
fi

python3 - "${ORIG}" "${DATOS}" <<'PY'
import sys
import wave
from pathlib import Path
import numpy as np

orig_dir = Path(sys.argv[1])
datos_dir = Path(sys.argv[2])
FS = 44100


def escribir_wav_cd(ruta: Path, muestras: np.ndarray) -> None:
    muestras = np.asarray(muestras, dtype=np.int16)
    assert muestras.ndim == 2 and muestras.shape[1] == 2
    with wave.open(str(ruta), "wb") as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(FS)
        w.writeframes(muestras.tobytes())
    print(f"Generado: {ruta.relative_to(datos_dir.parent)} ({len(muestras)/FS:.1f} s)")


def recortar_wav(origen: Path, destino: Path, inicio_s: float, duracion_s: float) -> bool:
    if not origen.exists():
        return False
    with wave.open(str(origen), "rb") as w:
        if w.getnchannels() != 2 or w.getsampwidth() != 2 or w.getframerate() != FS:
            return False
        ini = int(inicio_s * FS)
        n = int(duracion_s * FS)
        if ini + n > w.getnframes():
            ini = 0
            n = min(n, w.getnframes())
        w.setpos(ini)
        raw = w.readframes(n)
    datos = np.frombuffer(raw, dtype=np.int16).reshape(-1, 2)
    escribir_wav_cd(destino, datos)
    return True


def sintetizar_respaldo(semilla: int, freqs: list[float], duracion_s: float, ruido_amp: float) -> np.ndarray:
    rng = np.random.default_rng(semilla)
    n = int(duracion_s * FS)
    t = np.arange(n, dtype=np.float64) / FS
    canal_l = np.zeros(n, dtype=np.float64)
    canal_r = np.zeros(n, dtype=np.float64)
    for idx, f in enumerate(freqs):
        env = 0.6 + 0.4 * np.sin(2.0 * np.pi * (1.2 + 0.3 * idx) * t + rng.uniform(0, 6.28))
        canal_l += env * np.sin(2.0 * np.pi * f * t + rng.uniform(0, 6.28))
        canal_r += env * np.cos(2.0 * np.pi * f * 1.002 * t + rng.uniform(0, 6.28))
    canal_l = canal_l / max(np.max(np.abs(canal_l)), 1.0) * 0.55 + rng.standard_normal(n) * ruido_amp
    canal_r = canal_r / max(np.max(np.abs(canal_r)), 1.0) * 0.55 + rng.standard_normal(n) * ruido_amp
    estereo = np.column_stack([canal_l, canal_r])
    return np.clip(estereo * 32768.0, -32768, 32767).astype(np.int16)


# 1. Clásica (Vivaldi: versión de 20 s y versión de 10 s compatible con el módulo A)
if not recortar_wav(orig_dir / "vivaldi_full.wav", datos_dir / "clasica_vivaldi.wav", 85.0, 20.0):
    if not (datos_dir / "clasica_vivaldi.wav").exists():
        escribir_wav_cd(
            datos_dir / "clasica_vivaldi.wav",
            sintetizar_respaldo(101, [220.0, 277.18, 329.63, 440.0, 554.37, 659.25, 880.0], 20.0, 0.12),
        )

if not recortar_wav(orig_dir / "vivaldi_full.wav", datos_dir / "vivaldi_10s.wav", 85.0, 10.0):
    if (datos_dir / "clasica_vivaldi.wav").exists():
        recortar_wav(datos_dir / "clasica_vivaldi.wav", datos_dir / "vivaldi_10s.wav", 0.0, 10.0)

# 2. Ragtime / Acústico (Scott Joplin - The Entertainer, 20 s)
if not recortar_wav(orig_dir / "joplin_full.wav", datos_dir / "ragtime_joplin.wav", 5.0, 20.0):
    if not (datos_dir / "ragtime_joplin.wav").exists():
        escribir_wav_cd(
            datos_dir / "ragtime_joplin.wav",
            sintetizar_respaldo(202, [130.81, 164.81, 196.0, 261.63, 329.63, 392.0, 523.25], 20.0, 0.15),
        )

# 3. Jazz (Jazz at the Park, 20 s)
if not recortar_wav(orig_dir / "jazz_full.wav", datos_dir / "jazz_park.wav", 5.0, 20.0):
    if not (datos_dir / "jazz_park.wav").exists():
        escribir_wav_cd(
            datos_dir / "jazz_park.wav",
            sintetizar_respaldo(303, [110.0, 146.83, 174.61, 220.0, 261.63, 311.13, 392.0], 20.0, 0.18),
        )

# 4. Electrónica / Sintetizador (generada de forma determinista, 20 s)
escribir_wav_cd(
    datos_dir / "electronica_sintetica.wav",
    sintetizar_respaldo(404, [65.41, 130.81, 196.0, 261.63, 392.0, 523.25, 783.99, 1046.5], 20.0, 0.18),
)
PY
