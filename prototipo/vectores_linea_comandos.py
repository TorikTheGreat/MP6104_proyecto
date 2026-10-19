"""
Este script genera un vector de casos de prueba para la línea de comandos en
tests/vectores/linea_comandos_casos.txt (puede que sea un poco excesivo hacer
tantas pruebas para esto, pero de todas formas creo que es un buen hábito).

Tipo texto, una línea por elemento:
    caso
    arg <argumento>          uno por argumento, en orden (bytes tal cual)
    codigo=<0 o 2>
    mensaje=<texto>          solo si codigo=2: el mensaje de error
    <campo>=<valor>          solo si codigo=0, todos los campos en orden fijo;
    <campo>                  sin "=" si la opción no se dio (queda el por defecto)

Los casos válidos dan los dos retardos o ninguno, para que el vector no cambie
cuando cambian los valores por defecto (la comprobación t0 != t1 los usa).
"""

import random

import linea_comandos as lc
import parametros as p
from vectores import escribir

CAMPOS_METADATOS = ["titulo", "artista", "autor", "album", "anio", "invitado", "productor"]


def _casos():
    e, s = b"entrada.wav", b"salida.wav"
    r = [b"--retardo0", b"1000", b"--retardo1", b"1500"]
    casos = [
        [],
        [e],
        [e, s],
        [e, b"--titulo", b"Hola"],
        [e, s, b"--titulo", b"Hola"],
        [e, e, b"--titulo", b"Hola"],
        [e, s, b"--titulo=Hola", *r],
        [b"--titulo", b"Hola", e, s],
        [e, s, b"--titulo"],
        [e, s, b"--titulo", b"--artista", b"x"],
        [e, s, b"--titulo=-x"],
        [e, s, b"--titulo", b"-x"],
        [e, s, b"--titulo", b"-"],
        [e, s, b"--titulo", b""],
        [e, s, b"--titulo", b"x" * 255],
        [e, s, b"--titulo", b"x" * 256],
        [e, s, b"--titulo", b"a", b"--titulo", b"b"],
        [e, s, b"--titulo", b"", b"--titulo", b"ok"],
        [e, s, b"--titulo", b"\xff", b"--titulo", b"ok"],
        [e, s, b"--titulo", b"ok", b"--titulo", b""],
        [e, b"--retardo0", b"abc", b"--retardo0", b"1000", b"--retardo1", b"1500"],
        [e, b"--canal", b"centro", b"--retardo0", b"x"],  # dos errores: el primero en ORDEN
        [e, s, b"--titulo", "Canción Ñandú".encode()],
        [e, s, b"--titulo", b"inv\xe1lido"],
        [e, s, b"--titulo", b"con espacios y\nsalto"],
        [e, s, b"--titulo", b"t", b"--artista", b"a", b"--autor", b"u", b"--album", b"l",
         b"--anio", b"2026", b"--invitado", b"i", b"--productor", b"p"],
        [e, s, b"--desconocida", b"x"],
        [e, b"--desconocida"],
        [e, b"-h"],
        [e, b"--ayuda"],
        [b"--ayuda"],
        [b"--ayuda=si"],
        [b"--", b"--titulo"],
        [b"--titulo", b"x", b"--", b"-raro.wav", s],
        [e, b"--canal", b"derecho"],
        [e, b"--canal", b"izquierdo"],
        [e, b"--canal", b"centro"],
        [e, b"--canal=Derecho"],
        [e, b"--diagnostico", b"bits.csv"],
        [e, b"--diagnostico="],
        [e, *r],
        [e, b"--retardo0", b"1000", b"--retardo1", b"1005"],  # las dos dan 44 muestras
        [e, b"--retardo0", b"1500", b"--retardo1", b"1000"],
        [e, b"--retardo0", b"12", b"--retardo1", b"1000000"],
        [e, b"--retardo0", b"11", b"--retardo1", b"1000"],
        [e, b"--retardo0", b"0", b"--retardo1", b"1000"],
        [e, b"--retardo0", b"1000001", b"--retardo1", b"1000"],
        [e, b"--retardo0=-5", b"--retardo1", b"1000"],
        [e, b"--retardo0", b"-5", b"--retardo1", b"1000"],
        [e, b"--retardo0", b"1e3", b"--retardo1", b"1500"],
        [e, b"--retardo0", b" 12", b"--retardo1", b"1500"],
        [e, b"--retardo0", "١٢".encode(), b"--retardo1", b"1500"],
        [e, b"--retardo0", b"12\n", b"--retardo1", b"1500"],
        [e, b"--retardo0", b"1e9", b"--retardo1", b"1500"],
        [e, b"--retardo0", b"99999999999", b"--retardo1", b"1500"],
        [e, b"--retardo0", b"0x10", b"--retardo1", b"1500"],
        [e, b"--retardo0", b"1_000", b"--retardo1", b"1500"],
        [e, b"--retardo0", b"0001000", b"--retardo1", b"1500"],
        [e, b"--retardo0", b"2000", b"--retardo0", b"1000", b"--retardo1", b"1500"],
        [e, *r, b"--atenuacion0", b"30"],
        [e, *r, b"--atenuacion0", b"12.5", b"--atenuacion1", b"100"],
        [e, *r, b"--atenuacion0", b"12.5", b"--atenuacion1", b"99.999999"],
        [e, *r, b"--atenuacion0", b"0.000001"],
        [e, *r, b"--atenuacion0", b"0"],
        [e, *r, b"--atenuacion0", b"0.0"],
        [e, *r, b"--atenuacion0", b"100.000001"],
        [e, *r, b"--atenuacion0", b"nan"],
        [e, *r, b"--atenuacion0", b"inf"],
        [e, *r, b"--atenuacion0", b"30."],
        [e, *r, b"--atenuacion0", b".5"],
        [e, *r, b"--atenuacion0", b"1e1"],
        [e, *r, b"--atenuacion0", b"30,5"],
        [e, *r, b"--atenuacion0", b"1." + b"1" * 400],
        [e, *r, b"--atenuacion0", b"0.1", b"--atenuacion1", b"33.333333"],
        # Un solo retardo, igual al por defecto del otro: se rechaza con el resuelto.
        [e, b"--retardo0", str(p.RETARDO1_US).encode()],
        # Bordes de la forma de los números, con ceros a la izquierda.
        [e, b"--retardo0", b"00001000", b"--retardo1", b"1500"],
        [e, *r, b"--atenuacion0", b"0030"],
        [e, *r, b"--atenuacion0", b"030.5"],
        [e, *r, b"--atenuacion0", b"1.1234567"],
        # --ayuda no evita la validación de los demás valores.
        [b"--ayuda", b"--retardo0", b"x"],
    ]
    # Atenuaciones al azar: C++ debe dar el mismo double que la conversión con enteros.
    rng = random.Random(4)
    atenuaciones = []
    while len(atenuaciones) < 19:
        texto = f"{rng.randint(0, 100)}.{rng.randint(0, 999999):0{rng.randint(1, 6)}d}"[:10]
        if lc.FORMA_ATENUACION.fullmatch(texto.encode()) and 0 < float(texto) < 100:
            atenuaciones.append(texto.encode())
    casos += [[e, *r, b"--atenuacion0", a] for a in atenuaciones]
    return casos


def _valor_real(x):
    return format(x, ".17g")


def _lineas(args):
    lineas = ["caso"] + [b"arg " + a for a in args]
    r = lc.analizar(args)
    if isinstance(r, lc.ErrorUso):
        return lineas + [f"codigo={lc.ERROR_USO}", f"mensaje={r.mensaje}"]
    lineas += [f"codigo={lc.EXITO}", f"modo={r.modo}", b"entrada=" + r.entrada,
               b"salida=" + r.salida]
    for categoria, campo in enumerate(CAMPOS_METADATOS, start=1):
        dato = r.metadatos.get(categoria)
        lineas.append(campo if dato is None else campo.encode() + b"=" + dato)
    for campo, valor, formato in [("retardo0", r.retardo0_us, str),
                                  ("retardo1", r.retardo1_us, str),
                                  ("atenuacion0", r.atenuacion0, _valor_real),
                                  ("atenuacion1", r.atenuacion1, _valor_real)]:
        lineas.append(campo if valor is None else f"{campo}={formato(valor)}")
    lineas.append(f"canal={r.canal}")
    lineas.append("diagnostico" if r.diagnostico is None else b"diagnostico=" + r.diagnostico)
    return lineas


def generar(directorio):
    lineas = []
    for args in _casos():
        lineas += _lineas(args)
    escribir(directorio, "linea_comandos_casos", "texto", lineas, generador=__file__)
    escribir(directorio, "linea_comandos_uso", "texto", lc.USO.splitlines(), generador=__file__)
