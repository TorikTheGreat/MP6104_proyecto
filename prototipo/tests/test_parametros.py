"""Pruebas de prototipo/parametros.py."""

import unittest
from unittest import mock

import numpy as np

import parametros as p


class PruebasRetardos(unittest.TestCase):
    def test_bordes(self):
        self.assertEqual(p.muestras_desde_us(11), 0)  # inválido para la línea de comandos
        self.assertEqual(p.muestras_desde_us(12), 1)
        self.assertEqual(p.muestras_desde_us(34), 1)
        self.assertEqual(p.muestras_desde_us(1000), 44)
        self.assertEqual(p.muestras_desde_us(1500), 66)
        self.assertEqual(p.muestras_desde_us(1_000_000), 44100)

    def test_media_muestra_redondea_hacia_arriba(self):
        self.assertEqual(p.muestras_desde_us(5000), 221)    # 220.5
        self.assertEqual(p.muestras_desde_us(15000), 662)   # 661.5; round() daría 662 también
        self.assertEqual(p.muestras_desde_us(25000), 1103)  # 1102.5; round() daría 1102

    def test_fuera_de_rango_o_no_entero(self):
        for malo in (0, -1, 1_000_001):
            with self.assertRaises(ValueError):
                p.muestras_desde_us(malo)
        for malo in (1000.0, True, "1000"):
            with self.assertRaises(TypeError):
                p.muestras_desde_us(malo)
        self.assertEqual(p.muestras_desde_us(np.int64(1000)), 44)


class PruebasAmplitud(unittest.TestCase):
    def test_atenuacion_como_reduccion(self):  # la otra lectura de R22
        with mock.patch.object(p, "ATENUACION_ES_AMPLITUD", False):
            self.assertAlmostEqual(p.amplitud_desde_porcentaje(30), 0.7)

    def test_porcentaje(self):
        self.assertEqual(p.amplitud_desde_porcentaje(30), 0.3)
        self.assertEqual(p.amplitud_desde_porcentaje(50), 0.5)
        for malo in (0, -5, 100, 100.0001):  # el codificador acepta (0, 100)
            with self.assertRaises(ValueError):
                p.amplitud_desde_porcentaje(malo)


class PruebasEscala(unittest.TestCase):
    def test_saturacion(self):
        self.assertEqual(p.a_int16(1.0), 32767)
        self.assertEqual(p.a_int16(1.5), 32767)
        self.assertEqual(p.a_int16(-1.0), -32768)
        self.assertEqual(p.a_int16(-1.5), -32768)

    def test_trunca_hacia_cero(self):  # como el escritor de WAV del codificador
        self.assertEqual(p.a_int16(0.9 / 32768), 0)
        self.assertEqual(p.a_int16(-0.9 / 32768), 0)
        self.assertEqual(p.a_int16(1.9 / 32768), 1)
        self.assertEqual(p.a_int16(-1.9 / 32768), -1)

    def test_rechaza_no_finitos(self):
        for malo in (float("nan"), float("inf"), -float("inf")):
            with self.assertRaises(ValueError):
                p.a_int16(malo)
            with self.assertRaises(ValueError):
                p.a_int16(np.array([0.0, malo]))

    def test_retardo_minimo_util(self):
        self.assertEqual(p.muestras_desde_us(p.RETARDO_MIN_UTIL_US), 1)
        self.assertEqual(p.muestras_desde_us(p.RETARDO_MIN_UTIL_US - 1), 0)

    def test_ida_y_vuelta_exacta(self):
        m = np.arange(-32768, 32768, dtype=np.int16)
        np.testing.assert_array_equal(p.a_int16(p.a_flotante(m)), m)

    def test_escalar_y_arreglo_coinciden(self):
        y = np.random.default_rng(1).uniform(-1.2, 1.2, 1000)
        self.assertEqual(p.a_int16(y).dtype, np.int16)
        self.assertEqual(list(p.a_int16(y)), [p.a_int16(v) for v in y])


class PruebasVentana(unittest.TestCase):
    def test_largo_ventana_depende_del_mayor_retardo(self):
        self.assertEqual(p.largo_ventana(1000, 1500), p.K_VENTANA * 66)
        self.assertEqual(p.largo_ventana(1500, 1000), p.K_VENTANA * 66)

    def test_valores_por_defecto_validos(self):
        self.assertNotEqual(p.muestras_desde_us(p.RETARDO0_US), p.muestras_desde_us(p.RETARDO1_US))
        p.amplitud_desde_porcentaje(p.ATENUACION0)
        p.amplitud_desde_porcentaje(p.ATENUACION1)

    def test_categorias(self):
        self.assertEqual(sorted(p.NOMBRES), list(range(1, 8)))
        self.assertEqual(p.NOMBRES[p.ALBUM], "Álbum")


if __name__ == "__main__":
    unittest.main()
