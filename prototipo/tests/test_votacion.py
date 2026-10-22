"""
Pruebas de prototipo/votacion.py y de la votación dentro de metadatos.desde_bits.
"""

import random
import unittest
from unittest import mock

import metadatos as m
import parametros as p
import trama_registros as f
import votacion as v
from canal_simulado import canal, generador
from tests.test_metadatos import validos
from tests.test_trama_registros import SIETE

COPIA = f.codificar(SIETE)


def errores(a, b):
    return sum(x != y for x, y in zip(a, b))


def es_rotacion(a, b):
    """a es b empezando en otra posición: la copia votada es cíclica."""
    return len(a) == len(b) and any(a == b[k:] + b[:k] for k in range(len(b)))


class PruebasPeriodo(unittest.TestCase):
    def test_periodo_de_una_secuencia_limpia(self):
        self.assertEqual(v.estimar_periodo(COPIA * 4), len(COPIA))

    def test_prefiere_el_menor_periodo(self):
        corta = f.codificar({p.TITULO: "ab"})  # 56 bits
        self.assertEqual(v.estimar_periodo(corta * 12), len(corta))

    def test_con_ruido(self):
        bits = canal(COPIA * 5, generador(1), p_inversion=0.05)
        self.assertEqual(v.estimar_periodo(bits), len(COPIA))

    def test_sin_tres_copias_no_hay_periodo(self):
        self.assertIsNone(v.estimar_periodo(COPIA * 2 + COPIA[:500]))

    def test_bits_aleatorios_no_tienen_periodo(self):
        rng = random.Random(3)
        for _ in range(5):
            self.assertIsNone(v.estimar_periodo([rng.randint(0, 1) for _ in range(3000)]))


class PruebasVoto(unittest.TestCase):
    def test_limpia(self):
        self.assertEqual(v.votar_copias(COPIA * 3), COPIA)

    def test_el_voto_corrige_las_inversiones(self):
        bits = canal(COPIA * 9, generador(2), p_inversion=0.05)
        self.assertGreater(errores(bits[:len(COPIA)], COPIA), 30)
        self.assertLessEqual(errores(v.votar_copias(bits), COPIA), 2)

    def test_un_bit_borrado_corre_solo_su_copia(self):
        bits = COPIA * 5
        del bits[len(COPIA) + 300]  # dentro de la segunda copia
        self.assertTrue(es_rotacion(v.votar_copias(bits), COPIA))

    def test_un_bit_insertado_corre_solo_su_copia(self):
        bits = COPIA * 5
        bits.insert(2 * len(COPIA) + 10, 1 - bits[2 * len(COPIA) + 10])
        self.assertTrue(es_rotacion(v.votar_copias(bits), COPIA))

    def test_empate_gana_la_primera_copia(self):
        copias = [list(COPIA) for _ in range(4)]
        copias[0][5] = copias[1][5] = 1 - COPIA[5]  # 2 contra 2
        self.assertEqual(v.votar_copias(sum(copias, []))[5], 1 - COPIA[5])

    def test_gana_la_mayoria_aunque_la_primera_copia_este_mal(self):
        copias = [list(COPIA) for _ in range(4)]
        copias[0][7] = 1 - COPIA[7]  # 1 contra 3
        self.assertEqual(v.votar_copias(sum(copias, [])), COPIA)

    def test_sin_periodo_no_hay_voto(self):
        self.assertIsNone(v.votar_copias(COPIA * 2))


class PruebasDesdeBitsConVoto(unittest.TestCase):
    def test_votar_recupera_mas_con_5_por_ciento(self):
        bits = canal(m.a_bits(SIETE, 6 * len(COPIA)).bits, generador(5), p_inversion=0.05)
        with mock.patch.object(p, "VOTAR_COPIAS", False):
            sin_voto = validos(m.desde_bits(bits))
        self.assertGreater(len(validos(m.desde_bits(bits))), len(sin_voto) + 2)

    def test_recupera_todo_con_9_copias_y_5_por_ciento(self):
        bits = canal(m.a_bits(SIETE, 9 * len(COPIA)).bits, generador(5), p_inversion=0.05)
        self.assertEqual(validos(m.desde_bits(bits)), SIETE)

    def test_copia_cortada_al_principio(self):
        # Los bits empiezan a mitad de un registro: la copia votada es cíclica.
        bits = canal(m.a_bits(SIETE, 6 * len(COPIA)).bits[333:], generador(6),
                     p_inversion=0.03)
        self.assertEqual(validos(m.desde_bits(bits)), SIETE)

    def test_la_copia_votada_cuenta_una_vez(self):
        lecturas = m.leer_votada(COPIA * 3)
        self.assertEqual(len(lecturas), len(SIETE))


if __name__ == "__main__":
    unittest.main()
