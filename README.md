# Proyecto 1 — Esteganografía por enmascaramiento con eco

Curso **MP6104 — Procesamiento Digital de Señales**, Maestría en Electrónica,
Instituto Tecnológico de Costa Rica, IV Cuatrimestre 2026.

- **Integrantes:**
  - Marco Zolla Salazar
  - Por completar (rol A) <!-- TODO: cada integrante cambia solo su renglón -->
  - Por completar (rol B)
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

## Pruebas

```bash
make test                  # C++: compila bin/pruebas y corre todo
./bin/pruebas fft          # solo las pruebas con «fft» en el nombre
make prototipo-test        # Python: prototipo/tests/test_*.py
make vectores              # regenera los vectores de referencia Python → C++
```

**C++.** Cada módulo tiene su archivo `tests/pruebas_<módulo>.cpp`, que incluye
`tests/pruebas.hpp`; el Makefile los enlaza todos en `bin/pruebas` y
`tests/principal.cpp` tiene el único `main`. No hace falta registrar nada a mano:

```cpp
#include "pruebas.hpp"

PRUEBA(nombre_unico_y_descriptivo) {
    REQUERIR(v.size() == 3);                 // si falla, termina la prueba
    VERIFICAR(v[2] > 0);
    VERIFICAR_IGUAL(obtenido, esperado);     // exacta: enteros, texto, vectores
    VERIFICAR_CERCA(obtenido, esperado, 1e-9);  // reales y complejos
}
```

`tests/pruebas_ejemplo.cpp` sirve de plantilla. Los nombres de las pruebas son
únicos en todo el proyecto.

**Python.** Las pruebas van en `prototipo/tests/test_<módulo>.py` con `unittest`
e importan los módulos por su nombre (`import vectores`).

**Comparación con el prototipo (vectores de referencia).** El prototipo escribe
entradas y salidas en `tests/vectores/<módulo>_<caso>.txt`, y las pruebas de C++
las leen (`pruebas::leer_reales`, `leer_complejos`, `leer_enteros`, `leer_textos`)
y comparan con su propia implementación:

- Para comparar un módulo así, se escribe `prototipo/vectores_<módulo>.py` con una
  función `generar(directorio)` que llama a `vectores.escribir`;
  `prototipo/vectores_ejemplo.py` sirve de plantilla. El formato está en
  `prototipo/vectores.py`.
- Los vectores **se versionan**: después de cambiar el prototipo o un generador,
  `make vectores` y commit de `tests/vectores/`. La CI corre
  `python prototipo/vectores.py --verificar` y falla si no están al día.
- Enteros y texto se comparan exactos; reales, con una tolerancia relativa de
  1e-12 más 1e-12 del máximo absoluto del vector. Cada prueba de C++ elige su
  propia tolerancia según el algoritmo.

## Ejecución

```bash
# Codificar: hay al menos un metadato (R28)
./bin/ecoesteg original.wav codificado.wav --titulo "Canción" --artista "Banda"

# Decodificar: sin metadatos (R39), con los mismos parámetros de enmascaramiento
./bin/ecoesteg codificado.wav

./bin/ecoesteg --ayuda
```

Los archivos deben ser WAV PCM de 16 bits, 44.1 kHz, estéreo.

| Opción | Valor | Por omisión |
|---|---|---|
| `--titulo`, `--artista`, `--autor`, `--album`, `--anio`, `--invitado`, `--productor` | texto UTF-8 de 1 a 255 bytes | |
| `--retardo0`, `--retardo1` | retardo del eco del bit 0 o 1, en us: entero de 12 a 1 000 000 (al menos una muestra) | 1000, 1500 |
| `--atenuacion0`, `--atenuacion1` | amplitud del eco del bit 0 o 1, en %: (0, 100), con hasta 6 decimales | 30, 30 |
| `--canal` | `izquierdo` o `derecho` | `izquierdo` |
| `--diagnostico` | ruta de un CSV con el bit de cada ventana | |
| `--ayuda` | muestra el uso | |

- Se acepta `--opcion valor` y `--opcion=valor`, antes o después de los archivos;
  `--` termina las opciones. Un valor que empieza con `-` se escribe con `=`
  (`--titulo=-x`).
- Si una opción se repite, vale la última (R9). Una opción desconocida (R7) o un
  valor inválido termina el programa con un mensaje y el uso.
- La atenuación p es la amplitud del eco: el filtro del bit b es
  H_b(z) = 1 + (p_b/100) z^(-t_b) (interpretación de R22, pendiente de confirmar).
- Los dos retardos deben dar distinto número de muestras (1 muestra = 22.68 us).
- Al codificar, el archivo de salida no puede ser el mismo de entrada.
- Los metadatos se repiten hasta llenar el audio. Si no caben ni una vez, se
  codifica lo que cabe y se avisa en la salida de error cuántos bits se perdieron.
- Al decodificar, cada metadato se imprime en la salida estándar
  (`Título: Canción`, R48, R49). Los metadatos desconocidos o dañados se avisan
  en la salida de error (R50, R51, sección 4.3.4), por ejemplo
  `ADVERTENCIA: metadato desconocido (dañado): Canci?n`. También se avisa si no se
  encontró ninguno.

**Códigos de salida:** 0 éxito (también si el archivo no tenía metadatos o si los
metadatos no cupieron completos), 1 error de ejecución (archivo o formato WAV), 2
error de uso.

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
