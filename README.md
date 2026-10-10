# Proyecto 1 — Esteganografía por enmascaramiento con eco

Curso **MP6104 — Procesamiento Digital de Señales**, Maestría en Electrónica,
Instituto Tecnológico de Costa Rica, IV Cuatrimestre 2026.

- **Integrantes:** Marco Zolla Salazar <!-- TODO: completar el equipo -->
- **Profesor:** M.Sc. Carl Michael Grüner Monzón

Sistema que esconde metadatos de una canción (título, artista, autor, álbum, año,
artista invitado y productor) en un archivo WAV agregando ecos imperceptibles, y
los recupera mediante la autocorrelación del cepstro [Gruhl, Lu y Bender, *Echo
Hiding*]. El prototipo de los algoritmos está en Python y la implementación final
en C++.

---

## Contenido

| Ruta | Descripción |
|---|---|
| `pds_2026c4_proyecto1_enmascaramiento_eco.pdf` | Enunciado del proyecto. |
| `src/` | Implementación final en C++17 (`bin/ecoesteg`). |
| `tests/` | Pruebas de la implementación en C++ (`make test`). |
| `.github/workflows/ci.yml` | Integración continua: compila con `-Werror` y corre las pruebas de C++ y Python en cada push y pull request. |
| `prototipo/` | Prototipo de los algoritmos en Python, sus pruebas (`prototipo/tests/`) y experimentos (`experimentos_*.py`). |
| `articulo/` | Artículo científico (LaTeX, IEEE Transactions) y su PDF. |
| `datos/` | Fragmentos WAV de prueba, con fuente y licencia en `datos/README.md` (`make datos` los regenera). |
| `scripts/` | Descarga de datos y ejecución de experimentos. |
| `figuras/`, `resultados/` | Figuras y tablas que usa el artículo; `resultados/ejemplo_codificado.wav` es un ejemplo de archivo codificado (R38). |
| `Makefile` | Construcción del programa, pruebas, experimentos y artículo. |

---

## Dependencias

- **C++:** `g++` con soporte de C++17 y GNU Make.
- **Python** ≥ 3.12 con `numpy`, `scipy`, `matplotlib` y `pesq` (ver
  `prototipo/requirements.txt`), solo para el prototipo y los experimentos.
- **LaTeX:** TeX Live con `IEEEtran`, `babel-spanish`, `siunitx`, `booktabs`,
  `cleveref` y `latexmk`.

En Debian/Ubuntu:

```bash
sudo apt install build-essential python3-venv python3-dev latexmk \
                 texlive-latex-recommended texlive-latex-extra \
                 texlive-lang-spanish texlive-publishers texlive-science
```

## Instalación y construcción

```bash
git clone https://github.com/TorikTheGreat/MP6104_proyecto.git
cd MP6104_proyecto
make               # compila bin/ecoesteg
make test          # pruebas de C++
make venv          # entorno de Python para el prototipo y los experimentos
make prototipo-test  # pruebas del prototipo en Python
make help          # lista todos los objetivos
```

## Ejecución

<!-- TODO: completar con las opciones definitivas. -->

```bash
# Codificar: hay al menos un metadato (R28)
./bin/ecoesteg original.wav codificado.wav --titulo "..." --artista "..."

# Decodificar: sin metadatos (R39)
./bin/ecoesteg codificado.wav
```

Los archivos deben ser WAV PCM de 16 bits, 44.1 kHz, estéreo.

## Experimentos y artículo

```bash
make datos          # regenera los fragmentos WAV de prueba
make experimentos   # PESQ, tasa de recuperación y sus correlaciones
make articulo       # compila el artículo en articulo/build/
make pdf            # copia el PDF de entrega a articulo/articulo.pdf
```

## Limpieza

```bash
make clean          # borra build/, bin/ y temporales de LaTeX
make distclean      # además borra .venv/
```
