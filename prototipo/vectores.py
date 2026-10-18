"""Vectores de referencia Python → C++.

Un vector de referencia es un archivo de texto en tests/vectores/ con valores que
calculó el prototipo; las pruebas de C++ lo leen (tests/pruebas.hpp) y comparan
contra su propia implementación. Se versionan, así que la CI de C++ no necesita
Python.

Cada módulo que quiera compararse así tiene un generador
prototipo/vectores_<módulo>.py con una función ``generar(directorio)`` que llama
a ``escribir``. Este guion los encuentra solos.

Formato del archivo (UTF-8):
    # generado por prototipo/vectores_<módulo>.py, semilla N, tipo=real
    un valor por línea

    real      %.17g
    complejo  "re im" en una línea, cada parte con %.17g
    entero    decimal
    texto     una cadena por línea; se escriben como \\xNN los bytes < 0x20 (salvo
              el salto de línea, que es \\n), el 0x7F, los bytes que no son UTF-8
              válido y un "#" al inicio; la barra invertida es \\\\.
Solo la primera línea es cabecera.

Los generadores con números aleatorios usan np.random.default_rng con semilla
fija. NumPy garantiza la secuencia de bits de PCG64 entre versiones, pero no la
de sus métodos (integers, uniform, standard_normal): si --verificar falla al
cambiar la versión de numpy en requirements.txt sin otro cambio, basta con
regenerar los vectores.

Uso:
    python prototipo/vectores.py              regenera tests/vectores/
    python prototipo/vectores.py --verificar  falla si lo versionado no coincide
"""

import argparse
import importlib
import math
import sys
import tempfile
from pathlib import Path

RAIZ = Path(__file__).resolve().parent.parent
DIRECTORIO = RAIZ / "tests" / "vectores"
PREFIJO_CABECERA = "# generado por prototipo/vectores_"
TIPOS = ("real", "complejo", "entero", "texto")

# Tolerancia de --verificar para reales y complejos: NumPy elige rutinas
# distintas según el procesador, así que el último dígito puede variar.
TOL_RELATIVA = 1e-12
TOL_ABSOLUTA = 1e-12  # se multiplica por el máximo absoluto del vector


# ---- Escritura ------------------------------------------------------------

def escapar(valor):
    """Convierte una cadena (str o bytes) en una línea del formato texto."""
    if isinstance(valor, str):
        valor = valor.encode("utf-8")
    # surrogateescape convierte cada byte inválido en U+DC80..U+DCFF.
    caracteres = valor.decode("utf-8", errors="surrogateescape")
    partes = []
    for i, c in enumerate(caracteres):
        o = ord(c)
        if c == "\\":
            partes.append("\\\\")
        elif c == "\n":
            partes.append("\\n")
        elif 0xDC80 <= o <= 0xDCFF:
            partes.append(f"\\x{o - 0xDC00:02X}")
        elif o < 0x20 or o == 0x7F or (c == "#" and i == 0):
            partes.append(f"\\x{o:02X}")
        else:
            partes.append(c)
    return "".join(partes)


def _formatear(tipo, valor):
    if tipo == "real":
        return format(float(valor), ".17g")
    if tipo == "complejo":
        z = complex(valor)
        return f"{z.real:.17g} {z.imag:.17g}"
    if tipo == "entero":
        if isinstance(valor, float) or not float(valor).is_integer():
            raise TypeError(f"no es entero: {valor!r}")
        return str(int(valor))
    return escapar(valor)


def escribir(directorio, nombre, tipo, valores, *, generador, semilla=None):
    """Escribe directorio/<nombre>.txt.

    generador: ``__file__`` del módulo que llama (va en la cabecera).
    semilla: la del generador aleatorio, si se usó.
    """
    if tipo not in TIPOS:
        raise ValueError(f"tipo desconocido: {tipo!r} (válidos: {', '.join(TIPOS)})")
    ruta = Path(directorio) / f"{nombre}.txt"
    if ruta.exists():
        raise FileExistsError(f"{ruta.name} ya lo escribió otro generador")
    cabecera = f"# generado por prototipo/{Path(generador).name}"
    if semilla is not None:
        cabecera += f", semilla {semilla}"
    cabecera += f", tipo={tipo}"
    lineas = [cabecera] + [_formatear(tipo, v) for v in valores]
    ruta.write_text("\n".join(lineas) + "\n", encoding="utf-8")


# ---- Lectura (para --verificar y las pruebas de Python) -------------------

def desescapar(linea):
    """Inversa de escapar: devuelve bytes."""
    salida = bytearray()
    i = 0
    while i < len(linea):
        c = linea[i]
        if c != "\\":
            salida += c.encode("utf-8")
            i += 1
        elif linea[i + 1:i + 2] == "\\":
            salida += b"\\"
            i += 2
        elif linea[i + 1:i + 2] == "n":
            salida += b"\n"
            i += 2
        elif linea[i + 1:i + 2] == "x" and len(linea) >= i + 4:
            salida.append(int(linea[i + 2:i + 4], 16))
            i += 4
        else:
            raise ValueError(f"escape inválido en {linea!r}, posición {i}")
    return bytes(salida)


def leer(ruta):
    """Devuelve (cabecera, tipo, valores) de un vector."""
    texto = Path(ruta).read_text(encoding="utf-8")
    if not texto.endswith("\n"):
        raise ValueError(f"{ruta}: falta el salto de línea final")
    cabecera, *lineas = texto[:-1].split("\n")
    if ", tipo=" not in cabecera:
        raise ValueError(f"{ruta}: cabecera sin tipo")
    tipo = cabecera.rsplit(", tipo=", 1)[1]
    if tipo == "real":
        valores = [float(x) for x in lineas]
    elif tipo == "complejo":
        valores = [complex(*map(float, x.split(" "))) for x in lineas]
    elif tipo == "entero":
        valores = [int(x) for x in lineas]
    elif tipo == "texto":
        valores = [desescapar(x) for x in lineas]
    else:
        raise ValueError(f"{ruta}: tipo desconocido {tipo!r}")
    return cabecera, tipo, valores


# ---- Comparación ----------------------------------------------------------

def _componentes(tipo, valores):
    if tipo == "complejo":
        return [p for z in valores for p in (z.real, z.imag)]
    return list(valores)


def diferencias(ruta_a, ruta_b):
    """Lista de diferencias entre dos vectores (vacía si coinciden)."""
    cab_a, tipo, a = leer(ruta_a)
    cab_b, _, b = leer(ruta_b)
    if cab_a != cab_b:
        return [f"cabecera distinta: {cab_a!r} / {cab_b!r}"]
    if len(a) != len(b):
        return [f"{len(a)} valores / {len(b)}"]
    if tipo in ("entero", "texto"):
        return [f"valor {i}: {x!r} / {y!r}" for i, (x, y) in enumerate(zip(a, b)) if x != y]
    ca, cb = _componentes(tipo, a), _componentes(tipo, b)
    escala = max((abs(x) for x in ca + cb if math.isfinite(x)), default=0.0)
    salida = []
    for i, (x, y) in enumerate(zip(ca, cb)):
        if math.isnan(x) and math.isnan(y):
            continue
        if x == y:  # cubre los infinitos
            continue
        if not abs(x - y) <= TOL_RELATIVA * max(abs(x), abs(y)) + TOL_ABSOLUTA * escala:
            salida.append(f"componente {i}: {x!r} / {y!r}")
    return salida


# ---- Generación -----------------------------------------------------------

def generadores():
    """Módulos prototipo/vectores_*.py, en orden alfabético."""
    carpeta = Path(__file__).resolve().parent
    if str(carpeta) not in sys.path:
        sys.path.insert(0, str(carpeta))
    return [importlib.import_module(r.stem) for r in sorted(carpeta.glob("vectores_*.py"))]


def generar_todos(directorio):
    for modulo in generadores():
        modulo.generar(directorio)


def _propios(directorio):
    """Vectores de un directorio que escribió algún prototipo/vectores_*.py."""
    propios = {}
    for ruta in sorted(Path(directorio).glob("*.txt")):
        with open(ruta, encoding="utf-8") as f:
            if f.readline().startswith(PREFIJO_CABECERA):
                propios[ruta.name] = ruta
    return propios


def verificar(directorio=DIRECTORIO):
    """Regenera en un temporal y compara. Devuelve la lista de problemas."""
    problemas = []
    with tempfile.TemporaryDirectory() as tmp:
        generar_todos(tmp)
        nuevos = {r.name: r for r in Path(tmp).glob("*.txt")}
        versionados = _propios(directorio)
        for nombre in sorted(nuevos):
            if nombre not in versionados:
                problemas.append(f"{nombre}: no está en tests/vectores/ (corra make vectores)")
                continue
            for d in diferencias(versionados[nombre], nuevos[nombre]):
                problemas.append(f"{nombre}: {d}")
        for nombre in sorted(set(versionados) - set(nuevos)):
            problemas.append(f"{nombre}: ningún generador lo produce ya (bórrelo)")
    return problemas


def regenerar(directorio=DIRECTORIO):
    """Regenera los vectores y borra los propios que ya nadie genera."""
    directorio = Path(directorio)
    directorio.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory() as tmp:
        generar_todos(tmp)
        nuevos = {r.name: r for r in Path(tmp).glob("*.txt")}
        for nombre, ruta in _propios(directorio).items():
            if nombre not in nuevos:
                ruta.unlink()
                print(f"borrado {nombre} (ningún generador lo produce ya)")
        for nombre, ruta in sorted(nuevos.items()):
            (directorio / nombre).write_bytes(ruta.read_bytes())
    print(f"{len(nuevos)} vectores en {directorio.relative_to(RAIZ)}/")


def main():
    analizador = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    analizador.add_argument("--verificar", action="store_true",
                            help="comparar con tests/vectores/ en vez de escribir")
    args = analizador.parse_args()
    if not args.verificar:
        regenerar()
        return 0
    problemas = verificar()
    for p in problemas:
        print(p)
    if problemas:
        print("Los vectores versionados no coinciden con el prototipo.")
        return 1
    print("Vectores al día.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
