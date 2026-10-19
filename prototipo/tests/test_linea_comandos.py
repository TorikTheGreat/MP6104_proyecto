"""Pruebas de prototipo/linea_comandos.py."""

import io
import unittest
from contextlib import redirect_stderr, redirect_stdout

import linea_comandos as lc
import parametros as p


def analizar(*args):
    return lc.analizar([a.encode() if isinstance(a, str) else a for a in args])


class PruebasModo(unittest.TestCase):
    def test_codificar_y_decodificar(self):
        c = analizar("e.wav", "s.wav", "--titulo", "Hola")
        self.assertEqual((c.modo, c.entrada, c.salida), ("codificar", b"e.wav", b"s.wav"))
        self.assertEqual(c.metadatos, {p.TITULO: b"Hola"})
        c = analizar("e.wav")
        self.assertEqual((c.modo, c.entrada, c.metadatos), ("decodificar", b"e.wav", {}))

    def test_posicionales_equivocados(self):
        for args in ([], ["e.wav", "s.wav"], ["e.wav", "--titulo", "x"], ["a", "b", "c", "--titulo", "x"]):
            self.assertIsInstance(analizar(*args), lc.ErrorUso, args)

    def test_salida_igual_a_entrada(self):
        self.assertIsInstance(analizar("e.wav", "e.wav", "--titulo", "x"), lc.ErrorUso)

    def test_ayuda(self):
        self.assertEqual(analizar("--ayuda").modo, "ayuda")
        self.assertIsInstance(analizar("--ayuda=x"), lc.ErrorUso)

    def test_opciones_desconocidas(self):
        for args in (["e.wav", "--x"], ["e.wav", "-h"], ["e.wav", "--titulos", "a"]):
            self.assertIsInstance(analizar(*args), lc.ErrorUso, args)


class PruebasSintaxis(unittest.TestCase):
    def test_igual_y_separado(self):
        self.assertEqual(analizar("e", "s", "--titulo=a=b").metadatos[p.TITULO], b"a=b")
        self.assertEqual(analizar("--titulo", "a", "e", "s").metadatos[p.TITULO], b"a")

    def test_ultima_gana(self):  # R9
        self.assertEqual(analizar("e", "s", "--titulo", "a", "--titulo", "b").metadatos[p.TITULO], b"b")
        c = analizar("e", "--retardo0", "2000", "--retardo0", "1000", "--retardo1", "1500")
        self.assertEqual(c.retardo0_us, 1000)

    def test_solo_se_valida_la_ultima_repeticion(self):  # R9
        for primero in ("", "x" * 300, b"\xff"):
            c = analizar("e", "s", "--titulo", primero, "--titulo", "ok")
            self.assertEqual(c.metadatos[p.TITULO], b"ok", primero)
        c = analizar("e", "--retardo0", "abc", "--retardo0", "1000", "--retardo1", "1500")
        self.assertEqual(c.retardo0_us, 1000)
        self.assertIsInstance(analizar("e", "s", "--titulo", "ok", "--titulo", ""), lc.ErrorUso)

    def test_valor_que_parece_opcion(self):
        self.assertIsInstance(analizar("e", "s", "--titulo", "-x"), lc.ErrorUso)
        self.assertEqual(analizar("e", "s", "--titulo=-x").metadatos[p.TITULO], b"-x")
        self.assertEqual(analizar("e", "s", "--titulo", "-").metadatos[p.TITULO], b"-")

    def test_doble_guion(self):
        c = analizar("--titulo", "x", "--", "-raro.wav", "--s.wav")
        self.assertEqual((c.entrada, c.salida), (b"-raro.wav", b"--s.wav"))


class PruebasValores(unittest.TestCase):
    def test_metadatos(self):
        self.assertIsInstance(analizar("e", "s", "--titulo", b"inv\xe1lido"), lc.ErrorUso)
        self.assertIsInstance(analizar("e", "s", "--titulo", ""), lc.ErrorUso)
        self.assertIsInstance(analizar("e", "s", "--titulo", "x" * 256), lc.ErrorUso)
        self.assertEqual(analizar("e", "s", "--album", "Álbum").metadatos, {p.ALBUM: "Álbum".encode()})

    def test_retardos(self):
        c = analizar("e", "--retardo0", "12", "--retardo1", "1000000")
        self.assertEqual((c.retardo0_us, c.retardo1_us), (12, 1_000_000))
        for malo in ("11", "0", "1000001", "1e3", " 12", "١٢", "12\n", "99999999999", "+5", "0x10"):
            self.assertIsInstance(analizar("e", "--retardo0", malo, "--retardo1", "1500"), lc.ErrorUso, malo)

    def test_retardos_con_las_mismas_muestras(self):
        self.assertIsInstance(analizar("e", "--retardo0", "1000", "--retardo1", "1005"), lc.ErrorUso)

    def test_t0_igual_t1_con_valores_por_defecto(self):
        # Solo retardo0, igual al por defecto de retardo1: se rechaza con el resuelto.
        self.assertIsInstance(analizar("e", "--retardo0", str(p.RETARDO1_US)), lc.ErrorUso)

    def test_atenuaciones(self):
        c = analizar("e", "--atenuacion0", "12.5", "--atenuacion1", "99.999999")
        self.assertEqual((c.atenuacion0, c.atenuacion1), (12.5, 99.999999))
        for malo in ("0", "0.0", "100", "100.000001", "nan", "inf", "30.", ".5", "1e1", "-3", "1." + "1" * 400):
            self.assertIsInstance(analizar("e", "--atenuacion0", malo), lc.ErrorUso, malo)

    def test_atenuacion_igual_que_float(self):
        # La conversión con enteros da el mismo double que float() del texto.
        import random
        rng = random.Random(4)
        for _ in range(20000):
            texto = f"{rng.randint(0, 100)}.{rng.randint(0, 999999):0{rng.randint(1, 6)}d}"[:10]
            if not lc.FORMA_ATENUACION.fullmatch(texto.encode()) or not 0 < float(texto) < 100:
                continue
            c = analizar("e", "--atenuacion0", texto)
            self.assertEqual(c.atenuacion0, float(texto), texto)

    def test_por_defecto_quedan_sin_dar(self):
        c = analizar("e")
        self.assertEqual((c.retardo0_us, c.atenuacion0, c.canal, c.diagnostico),
                         (None, None, "izquierdo", None))
        self.assertEqual(lc.resolver(c),
                         (p.RETARDO0_US, p.RETARDO1_US, p.ATENUACION0, p.ATENUACION1))

    def test_canal_y_diagnostico(self):
        self.assertEqual(analizar("e", "--canal", "derecho").canal, "derecho")
        self.assertIsInstance(analizar("e", "--canal", "centro"), lc.ErrorUso)
        self.assertEqual(analizar("e", "--diagnostico", "d.csv").diagnostico, b"d.csv")
        self.assertIsInstance(analizar("e", "--diagnostico="), lc.ErrorUso)


class PruebasPrincipal(unittest.TestCase):
    def ejecutar(self, *argv):
        salida, errores = io.StringIO(), io.StringIO()
        with redirect_stdout(salida), redirect_stderr(errores):
            codigo = lc.principal(list(argv))
        return codigo, salida.getvalue(), errores.getvalue()

    def test_codigos_de_salida(self):
        codigo, salida, _ = self.ejecutar("--ayuda")
        self.assertEqual(codigo, lc.EXITO)
        self.assertIn("Uso:", salida)
        codigo, _, errores = self.ejecutar("e.wav", "--canal", "centro")
        self.assertEqual(codigo, lc.ERROR_USO)
        self.assertTrue(errores.startswith("error: --canal"))
        self.assertIn("Uso:", errores)


if __name__ == "__main__":
    unittest.main()
