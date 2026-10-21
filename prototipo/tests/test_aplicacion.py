"""
Pruebas para prototipo/aplicacion.py. Sirve para hacer una ejecución del flujo 
completo con bloques falsos (sin audio) y una de punta a punta con los bloques 
reales sobre un WAV sintético.
"""

import io
import os
import tempfile
import unittest

import numpy as np

import aplicacion as ap
import bloques_reales
import linea_comandos as lc
import metadatos as m
from parametros import ANIO, TITULO

LARGO = 25 * 66  # la ventana con los valores por defecto


class Falsos:
    """Bloques que guardan lo que reciben; el "disco" es un diccionario."""

    def __init__(self):
        self.archivos = {}
        self.bits_codificados = None
        self.canal_codificado = None
        self.escribio_izquierdo = None
        self.bits_a_decodificar = []
        self.canal_decodificado = None

    def leer(self, ruta):
        if ruta not in self.archivos:
            raise RuntimeError(f"No se pudo abrir: {ruta.decode()}")
        return self.archivos[ruta]

    def escribir(self, ruta, original, codificado, izquierdo):
        izq, der = original.izquierdo.copy(), original.derecho.copy()
        canal = izq if izquierdo else der
        canal[:] = codificado[:len(canal)].astype(np.int16)
        self.archivos[ruta] = ap.Audio(izq, der)
        self.escribio_izquierdo = izquierdo
        return 0

    def codificar(self, canal, bits, eco):
        self.bits_codificados = bits
        self.canal_codificado = canal
        return canal.astype(np.float64)

    def decodificar(self, canal, eco):
        self.canal_decodificado = canal
        return ap.Decodificacion(list(self.bits_a_decodificar), [
            ap.Ventana(k, b, float(b == 0), float(b == 1), 1.0)
            for k, b in enumerate(self.bits_a_decodificar)])

    def bloques(self):
        return ap.Bloques(self.leer, self.escribir, self.codificar, self.decodificar)


def audio(ventanas, izq=100, der=-7):
    return ap.Audio(np.full(ventanas * LARGO, izq, dtype=np.int16),
                    np.full(ventanas * LARGO, der, dtype=np.int16))


def correr(args, bloques):
    salida, error = io.StringIO(), io.StringIO()
    codigo = ap.ejecutar([a.encode() if isinstance(a, str) else a for a in args],
                         salida, error, bloques)
    return codigo, salida.getvalue(), error.getvalue()


class PruebasModos(unittest.TestCase):
    def test_error_de_uso(self):
        codigo, salida, error = correr([], Falsos().bloques())
        self.assertEqual(codigo, lc.ERROR_USO)
        self.assertTrue(error.startswith("error: "))
        self.assertIn("Uso:", error)
        self.assertEqual(salida, "")

    def test_ayuda(self):
        self.assertEqual(correr(["--ayuda"], Falsos().bloques())[:2], (lc.EXITO, lc.USO))

    def test_archivo_que_no_se_puede_leer(self):
        codigo, _, error = correr(["no_existe.wav"], Falsos().bloques())
        self.assertEqual(codigo, lc.ERROR_EJECUCION)
        self.assertIn("No se pudo abrir: no_existe.wav", error)


class PruebasCodificar(unittest.TestCase):
    def test_el_mensaje_se_repite_en_todas_las_ventanas(self):
        f = Falsos()
        f.archivos[b"e.wav"] = audio(300)
        codigo, _, error = correr(["e.wav", "s.wav", "--titulo", "Hola"], f.bloques())
        self.assertEqual((codigo, error), (lc.EXITO, ""))
        self.assertIn(b"s.wav", f.archivos)
        self.assertTrue(f.escribio_izquierdo)
        self.assertEqual(f.bits_codificados, m.a_bits({TITULO: "Hola"}, 300).bits)

    def test_canal_derecho(self):
        f = Falsos()
        f.archivos[b"e.wav"] = audio(300)
        correr(["e.wav", "s.wav", "--titulo", "Hola", "--canal", "derecho"], f.bloques())
        self.assertFalse(f.escribio_izquierdo)
        self.assertEqual(f.canal_codificado[0], -7)

    def test_mensaje_que_no_cabe_se_trunca_y_avisa(self):
        f = Falsos()
        f.archivos[b"e.wav"] = audio(30)  # 30 bits; "Hola" necesita 72
        codigo, _, error = correr(["e.wav", "s.wav", "--titulo", "Hola"], f.bloques())
        self.assertEqual(codigo, lc.EXITO)
        self.assertIn("Se codificaron 30 de 72 bits", error)
        self.assertIn(b"s.wav", f.archivos)

    def test_audio_sin_ninguna_ventana(self):
        f = Falsos()
        f.archivos[b"e.wav"] = ap.Audio(np.full(100, 5, np.int16), np.full(100, 5, np.int16))
        codigo, _, error = correr(["e.wav", "s.wav", "--titulo", "Hola"], f.bloques())
        self.assertEqual(codigo, lc.EXITO)
        self.assertIn("Se codificaron 0 de 72 bits", error)
        self.assertIsNone(f.bits_codificados)  # no se llamó al codificador
        self.assertIn(b"s.wav", f.archivos)

    def test_ventanas_en_silencio_no_llevan_bit(self):
        f = Falsos()
        a = audio(300)
        a.izquierdo[5 * LARGO:6 * LARGO] = 0  # ventana 5
        f.archivos[b"e.wav"] = a
        correr(["e.wav", "s.wav", "--titulo", "Hola"], f.bloques())
        mensaje = m.a_bits({TITULO: "Hola"}, 299).bits
        self.assertEqual(f.bits_codificados[4:7], [mensaje[4], -1, mensaje[5]])

    def test_diagnostico(self):
        f = Falsos()
        f.archivos[b"e.wav"] = audio(100)
        with tempfile.TemporaryDirectory() as d:
            csv = os.path.join(d, "c.csv")
            correr(["e.wav", "s.wav", "--titulo", "Hola", "--diagnostico", csv], f.bloques())
            with open(csv, encoding="utf-8") as archivo:
                self.assertTrue(archivo.read().startswith("ventana,bit\n0,1\n"))


class PruebasDecodificar(unittest.TestCase):
    def test_decodifica_y_notifica(self):
        f = Falsos()
        f.archivos[b"c.wav"] = audio(10)
        f.bits_a_decodificar = m.a_bits({TITULO: "Hola", ANIO: "2026"}, 400).bits
        self.assertEqual(correr(["c.wav"], f.bloques()), (lc.EXITO, "Título: Hola\nAño: 2026\n", ""))

    def test_canal_pedido(self):
        f = Falsos()
        f.archivos[b"c.wav"] = audio(10)
        correr(["c.wav", "--canal", "derecho"], f.bloques())
        self.assertEqual(f.canal_decodificado[0], -7)

    def test_sin_metadatos(self):
        f = Falsos()
        f.archivos[b"c.wav"] = audio(10)
        self.assertEqual(correr(["c.wav"], f.bloques()),
                         (lc.EXITO, "", "ADVERTENCIA: no se encontraron metadatos\n"))

    def test_diagnostico(self):
        f = Falsos()
        f.archivos[b"c.wav"] = audio(10)
        f.bits_a_decodificar = [1, 0]
        with tempfile.TemporaryDirectory() as d:
            csv = os.path.join(d, "d.csv")
            correr(["c.wav", "--diagnostico", csv], f.bloques())
            with open(csv, encoding="utf-8") as archivo:
                self.assertEqual(archivo.read(), "ventana,bit,r0,r1,rms\n0,1,0,1,1\n1,0,1,0,1\n")

    def test_diagnostico_en_ruta_invalida(self):
        f = Falsos()
        f.archivos[b"c.wav"] = audio(10)
        self.assertEqual(correr(["c.wav", "--diagnostico", "/no/existe/d.csv"], f.bloques())[0],
                         lc.ERROR_EJECUCION)


class PruebasPuntaAPunta(unittest.TestCase):
    def test_wav_sintetico(self):
        # 20 s de ruido en los dos canales: el decodificador lo lee casi sin errores.
        rng = np.random.default_rng(11)
        n = 20 * 44100
        a = ap.Audio(*(np.clip(rng.normal(0, 3000, n), -32768, 32767).astype(np.int16)
                       for _ in range(2)))
        reales = bloques_reales.bloques_reales()
        with tempfile.TemporaryDirectory() as d:
            original = os.path.join(d, "original.wav").encode()
            codificado = os.path.join(d, "codificado.wav").encode()
            reales.escribir(original, a, a.izquierdo.astype(np.float64), True)
            codigo, _, _ = correr([original, codificado, "--titulo", "Canción", "--anio", "2026"],
                                  reales)
            self.assertEqual(codigo, lc.EXITO)
            self.assertEqual(correr([codificado], reales)[:2],
                             (lc.EXITO, "Título: Canción\nAño: 2026\n"))
            # El otro canal queda como estaba (R26), y un archivo sin codificar no da nada.
            self.assertEqual(correr([codificado, "--canal", "derecho"], reales)[1], "")
            self.assertEqual(correr([original], reales)[1], "")


if __name__ == "__main__":
    unittest.main()
