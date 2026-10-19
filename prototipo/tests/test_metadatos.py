"""Pruebas de prototipo/metadatos.py: repetición, truncado y qué se reporta."""

import random
import unittest
from unittest import mock

import metadatos as m
import parametros as p
import trama_registros as f
from parametros import ARTISTA, AUTOR, TITULO
from trama import DANADO, DESCONOCIDO, VALIDO
from tests.test_trama_registros import SIETE, registro


def validos(resultados):
    return {c: d.decode() for t, c, d in resultados if t == VALIDO}


class PruebasCodificar(unittest.TestCase):
    def test_repetir(self):
        self.assertEqual(m.repetir([1, 0, 1], 7), [1, 0, 1, 1, 0, 1, 1])
        self.assertEqual(m.repetir([1, 0], 0), [])
        with self.assertRaises(ValueError):
            m.repetir([], 3)

    def test_llena_las_ventanas_repitiendo(self):
        copia = f.codificar({TITULO: "Hola"})
        r = m.a_bits({TITULO: "Hola"}, 200)
        self.assertEqual((len(r.bits), r.necesarios, r.truncado), (200, 72, False))
        self.assertEqual(r.bits, m.repetir(copia, 200))

    def test_sin_repetir_va_una_vez(self):
        with mock.patch.object(p, "REPETIR_MENSAJE", False):
            r = m.a_bits({TITULO: "Hola"}, 200)
        self.assertEqual(r.bits, f.codificar({TITULO: "Hola"}))

    def test_no_cabe_trunca(self):
        r = m.a_bits({TITULO: "Hola"}, 50)
        self.assertEqual((len(r.bits), r.necesarios, r.truncado), (50, 72, True))

    def test_no_cabe_con_error(self):
        with mock.patch.object(p, "SI_NO_CABE", "error"):
            with self.assertRaises(m.NoCabe) as e:
                m.a_bits({TITULO: "Hola"}, 50)
        self.assertEqual((e.exception.necesarios, e.exception.disponibles), (72, 50))

    def test_bits_necesarios(self):
        self.assertEqual(m.bits_necesarios({TITULO: "Hola"}), 72)


class PruebasDesdeBits(unittest.TestCase):
    def test_ida_y_vuelta(self):
        bits = m.a_bits(SIETE, 3 * m.bits_necesarios(SIETE) + 50).bits
        self.assertEqual(validos(m.desde_bits(bits)), SIETE)

    def test_texto_mas_frecuente_gana(self):
        bits = registro(TITULO, b"malo") + registro(TITULO, b"bueno") * 2
        self.assertEqual(m.desde_bits(bits), [(VALIDO, TITULO, b"bueno")])

    def test_danado_se_avisa_aunque_aparezca_una_vez(self):  # R50, sección 4.3.4
        malo = registro(TITULO, "Canción".encode(), crc=0)
        bueno = registro(ARTISTA, b"Banda")
        self.assertEqual(m.desde_bits(bueno + malo), [
            (VALIDO, ARTISTA, b"Banda"),
            (DANADO, TITULO, "Canción".encode()),
        ])

    def test_danado_de_una_categoria_ya_valida_no_se_avisa(self):
        bits = registro(TITULO, b"bien") + registro(TITULO, b"mal", crc=0)
        self.assertEqual(m.desde_bits(bits), [(VALIDO, TITULO, b"bien")])

    def test_danados_iguales_se_avisan_una_vez(self):
        bueno = registro(ARTISTA, b"Banda")
        malo = registro(TITULO, b"x", crc=0)
        self.assertEqual(m.desde_bits((bueno + malo) * 3),
                         [(VALIDO, ARTISTA, b"Banda"), (DANADO, TITULO, b"x")])

    def test_danados_con_el_mismo_texto_se_avisan_una_vez(self):
        # El byte de categoría de un dañado no es confiable: se agrupan por texto.
        bueno = registro(AUTOR, b"ok")
        bits = bueno + registro(TITULO, b"x", crc=0) + bueno + registro(ARTISTA, b"x", crc=0)
        self.assertEqual(m.desde_bits(bits), [(VALIDO, AUTOR, b"ok"), (DANADO, TITULO, b"x")])

    def test_desconocidos(self):
        bits = registro(TITULO, b"a") + registro(9, b"raro") + registro(9, b"raro")
        self.assertEqual(m.desde_bits(bits),
                         [(VALIDO, TITULO, b"a"), (DESCONOCIDO, 9, b"raro")])

    def test_sin_ningun_valido_no_se_avisa(self):
        bits = registro(9, b"raro") + registro(TITULO, b"x", crc=0)
        self.assertEqual(m.desde_bits(bits), [])

    def test_sin_ningun_valido_se_avisa_si_se_configura(self):
        bits = registro(9, b"raro") + registro(TITULO, b"x", crc=0)
        with mock.patch.object(p, "AVISOS_REQUIEREN_VALIDO", False):
            self.assertEqual([t for t, _, _ in m.desde_bits(bits)], [DESCONOCIDO, DANADO])
        bits = registro(TITULO, b"x", crc=0)
        with mock.patch.object(p, "AVISOS_REQUIEREN_VALIDO", False):
            self.assertEqual(m.desde_bits(bits), [(DANADO, TITULO, b"x")])

    def test_bits_aleatorios_no_producen_nada(self):
        rng = random.Random(3)
        for _ in range(20):
            bits = [rng.randint(0, 1) for _ in range(1000)]
            self.assertEqual(m.desde_bits(bits), [])

    def test_formato_se_elige_por_parametro(self):
        with mock.patch.object(p, "FORMATO_TRAMA", "no existe"):
            with self.assertRaises(KeyError):
                m.a_bits({TITULO: "a"}, 100)


if __name__ == "__main__":
    unittest.main()
