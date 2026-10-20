"""Pruebas de prototipo/trama_registros.py (el formato "registros")."""

import random
import unittest

import trama_registros as f
from parametros import ALBUM, ANIO, ARTISTA, AUTOR, INVITADO, PRODUCTOR, TITULO
from trama import DANADO, DESCONOCIDO, VALIDO

SIETE = {
    TITULO: "Canción de prueba",
    ARTISTA: "Ñandú y los Pingüinos",
    AUTOR: "José Pérez",
    ALBUM: "Álbum único",
    ANIO: "2026",
    INVITADO: "Zoë",
    PRODUCTOR: "Estudio Ártico",
}


def registro(categoria, datos, crc=None):
    """Bits de un registro, con el CRC dado o el correcto."""
    cuerpo = bytes([categoria, len(datos)]) + datos
    crc = f.crc16(cuerpo) if crc is None else crc
    return f.bits_de_bytes(bytes([f.PREAMBULO]) + cuerpo + bytes([crc >> 8, crc & 0xFF]))


def tipos(lecturas):
    return [(t, c) for t, c, _ in lecturas]


class PruebasCodificar(unittest.TestCase):
    def test_crc_valor_de_prueba(self):
        self.assertEqual(f.crc16(b"123456789"), 0x29B1)

    def test_un_registro(self):
        bits = f.codificar({TITULO: "A"})
        self.assertEqual(len(bits), 48)
        self.assertEqual(bits, registro(TITULO, b"A"))

    def test_largo_de_una_copia(self):
        self.assertEqual(len(f.codificar({TITULO: "Hola"})), 72)
        self.assertEqual(len(f.codificar(SIETE)),
                         sum(40 + 8 * len(t.encode()) for t in SIETE.values()))

    def test_orden_por_categoria(self):
        self.assertEqual(f.codificar({ARTISTA: "b", TITULO: "a"}),
                         f.codificar({TITULO: "a"}) + f.codificar({ARTISTA: "b"}))

    def test_rechaza_vacio_largo_y_categoria(self):
        for malo in ({}, {TITULO: ""}, {TITULO: "x" * 256}, {8: "x"}):
            with self.assertRaises(ValueError):
                f.codificar(malo)
        f.codificar({TITULO: "x" * 255})


class PruebasLeer(unittest.TestCase):
    def test_ida_y_vuelta(self):
        lecturas = f.leer(f.codificar(SIETE))
        self.assertEqual({c: d.decode() for t, c, d in lecturas if t == VALIDO}, SIETE)

    def test_desde_cualquier_desfase(self):
        copia = f.codificar(SIETE)
        bits = copia * 3
        for corte in (1, 7, 100, 333):
            leidos = {c for t, c, _ in f.leer(bits[corte:]) if t == VALIDO}
            self.assertEqual(leidos, set(SIETE))

    def test_bit_borrado_cuesta_un_solo_registro(self):
        copia = f.codificar({TITULO: "uno", ARTISTA: "dos", AUTOR: "tres"})
        fin_segundo = 2 * f.BITS_SOBRECARGA + 8 * 6
        danada = copia[:fin_segundo - 1] + copia[fin_segundo:]
        leidos = tipos(f.leer(danada + copia))
        self.assertEqual(leidos.count((VALIDO, ARTISTA)), 1)
        self.assertEqual(leidos.count((VALIDO, AUTOR)), 2)
        self.assertEqual(leidos.count((VALIDO, TITULO)), 2)

    def test_bit_insertado_cuesta_un_solo_registro(self):
        copia = f.codificar({TITULO: "uno", ARTISTA: "dos", AUTOR: "tres"})
        danada = copia[:60] + [1] + copia[60:]
        leidos = tipos(f.leer(danada + copia))
        self.assertEqual(leidos.count((VALIDO, TITULO)), 1)
        self.assertEqual(leidos.count((VALIDO, ARTISTA)), 2)

    def test_falso_preambulo_dentro_del_texto(self):
        # "²" es C2 B2 en UTF-8: el B2 seguido de 01 05 parece un registro de Título.
        texto = "x²\x01\x05ñ"
        self.assertIn(bytes([0xB2, 0x01, 0x05]), texto.encode())
        bits = f.codificar({TITULO: texto, ARTISTA: "y"}) * 3
        for corte in (0, 5, 30):
            validos = {c: d.decode() for t, c, d in f.leer(bits[corte:]) if t == VALIDO}
            self.assertEqual(validos, {TITULO: texto, ARTISTA: "y"})

    def test_longitud_cero_o_que_no_cabe_no_es_registro(self):
        cero = f.bits_de_bytes(bytes([0xB2, TITULO, 0, 0x12, 0x34])) + [0] * 40
        no_cabe = f.bits_de_bytes(bytes([0xB2, TITULO, 200])) + [0] * 40
        self.assertEqual(f.leer(cero), [])
        self.assertEqual(f.leer(no_cabe), [])

    def test_copia_final_cortada(self):
        copia = f.codificar({TITULO: "uno", ARTISTA: "dos"})
        self.assertEqual(tipos(f.leer(copia + copia[:-10])),
                         [(VALIDO, TITULO), (VALIDO, ARTISTA), (VALIDO, TITULO)])

    def test_tipos(self):
        bits = (registro(TITULO, b"a") + registro(9, b"raro") + registro(ARTISTA, b"\xff")
                + registro(AUTOR, b"x", crc=0))
        self.assertEqual(f.leer(bits), [
            (VALIDO, TITULO, b"a"),
            (DESCONOCIDO, 9, b"raro"),
            (DESCONOCIDO, ARTISTA, b"\xff"),
            (DANADO, AUTOR, b"x"),
        ])

    def test_crc_incorrecto_fuera_de_secuencia_no_se_anota(self):
        # Tras un registro dañado, el siguiente no empieza "en secuencia".
        bits = registro(TITULO, b"a", crc=0) + registro(ARTISTA, b"b", crc=0)
        self.assertEqual(tipos(f.leer(bits)), [(DANADO, TITULO)])

    def test_bits_aleatorios_casi_nunca_dan_validos(self):
        rng = random.Random(3)
        for _ in range(20):
            bits = [rng.randint(0, 1) for _ in range(1000)]
            self.assertNotIn(VALIDO, [t for t, _, _ in f.leer(bits)])


if __name__ == "__main__":
    unittest.main()
