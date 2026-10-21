"""Pruebas de prototipo/canal_simulado.py y de la trama con errores."""

import unittest

import metadatos as m
from canal_simulado import canal, generador
from tests.test_metadatos import validos
from tests.test_trama_registros import SIETE


class PruebasCanal(unittest.TestCase):
    def test_sin_errores_no_cambia_nada(self):
        bits = [1, 0, 1, 1, 0] * 20
        self.assertEqual(canal(bits, generador(1)), bits)

    def test_inversion_cambia_bits_sin_cambiar_el_largo(self):
        bits = [0, 1] * 500
        recibidos = canal(bits, generador(4), p_inversion=0.1)
        self.assertEqual(len(recibidos), len(bits))
        self.assertNotEqual(recibidos, bits)

    def test_borrado_e_insercion_cambian_el_largo(self):
        bits = [0, 1] * 500
        self.assertLess(len(canal(bits, generador(2), p_borrado=0.1)), len(bits))
        self.assertGreater(len(canal(bits, generador(3), p_insercion=0.1)), len(bits))

    def test_trama_con_pocos_errores_recupera_todo(self):
        bits = m.a_bits(SIETE, 6 * m.bits_necesarios(SIETE)).bits
        recibidos = canal(bits, generador(5), p_inversion=0.001)
        self.assertEqual(validos(m.desde_bits(recibidos)), SIETE)


if __name__ == "__main__":
    unittest.main()
