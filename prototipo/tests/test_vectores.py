"""Pruebas de prototipo/vectores.py: formato, escapes y verificación."""

import tempfile
import unittest
from pathlib import Path

import vectores


class PruebasEscapes(unittest.TestCase):
    def test_texto_comun_no_cambia(self):
        self.assertEqual(vectores.escapar("Canción Ñandú"), "Canción Ñandú")

    def test_escapes(self):
        self.assertEqual(vectores.escapar("a\nb"), "a\\nb")
        self.assertEqual(vectores.escapar("a\\b"), "a\\\\b")
        self.assertEqual(vectores.escapar("\t\x7f"), "\\x09\\x7F")
        self.assertEqual(vectores.escapar("#x#"), "\\x23x#")
        self.assertEqual(vectores.escapar(b"\xe1"), "\\xE1")  # Latin-1, no UTF-8

    def test_ida_y_vuelta(self):
        for valor in [b"", "Canción".encode(), b"\xff\xfe\\n", b"#\n#", bytes(range(256))]:
            self.assertEqual(vectores.desescapar(vectores.escapar(valor)), valor)

    def test_escape_invalido(self):
        with self.assertRaises(ValueError):
            vectores.desescapar("a\\qb")


class PruebasArchivos(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.dir = Path(self._tmp.name)

    def tearDown(self):
        self._tmp.cleanup()

    def test_cabecera_y_lectura(self):
        vectores.escribir(self.dir, "m_caso", "complejo", [1 + 2j, -0.5j],
                          generador="/x/prototipo/vectores_m.py", semilla=7)
        cabecera, tipo, valores = vectores.leer(self.dir / "m_caso.txt")
        self.assertEqual(cabecera, "# generado por prototipo/vectores_m.py, semilla 7, tipo=complejo")
        self.assertEqual(tipo, "complejo")
        self.assertEqual(valores, [1 + 2j, -0.5j])

    def test_real_con_17_digitos_es_exacto(self):
        vectores.escribir(self.dir, "r", "real", [0.1, 1 / 3], generador="vectores_m.py")
        self.assertEqual(vectores.leer(self.dir / "r.txt")[2], [0.1, 1 / 3])

    def test_entero_rechaza_reales(self):
        with self.assertRaises(TypeError):
            vectores.escribir(self.dir, "e", "entero", [1.5], generador="vectores_m.py")

    def test_dos_generadores_no_pueden_escribir_el_mismo_vector(self):
        vectores.escribir(self.dir, "e", "entero", [1], generador="vectores_a.py")
        with self.assertRaises(FileExistsError):
            vectores.escribir(self.dir, "e", "entero", [1], generador="vectores_b.py")

    def _par(self, a, b, tipo="real"):
        """Escribe a y b como el mismo vector en dos directorios nuevos y los compara."""
        da = Path(tempfile.mkdtemp(dir=self.dir))
        db = Path(tempfile.mkdtemp(dir=self.dir))
        vectores.escribir(da, "v", tipo, a, generador="vectores_m.py")
        vectores.escribir(db, "v", tipo, b, generador="vectores_m.py")
        return vectores.diferencias(da / "v.txt", db / "v.txt")

    def test_tolerancia_de_reales(self):
        self.assertEqual(self._par([1.0, 0.0], [1.0 + 1e-15, 1e-14]), [])
        self.assertNotEqual(self._par([1.0], [1.0 + 1e-9]), [])
        self.assertNotEqual(self._par([1.0], [1.0, 2.0]), [])

    def test_enteros_y_texto_exactos(self):
        self.assertNotEqual(self._par([1], [2], "entero"), [])
        self.assertNotEqual(self._par(["á"], ["a"], "texto"), [])


class PruebasVersionados(unittest.TestCase):
    def test_los_vectores_versionados_estan_al_dia(self):
        # Es lo mismo que corre la CI con --verificar.
        self.assertEqual(vectores.verificar(), [])


if __name__ == "__main__":
    unittest.main()
