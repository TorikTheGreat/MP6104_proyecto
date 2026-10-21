"""Pruebas de prototipo/notificacion.py."""

import unittest

import notificacion as n
from parametros import ARTISTA, TITULO
from trama import DANADO, DESCONOCIDO, VALIDO


class PruebasNotificar(unittest.TestCase):
    def test_lineas(self):
        salida, errores = n.notificar([
            (VALIDO, ARTISTA, b"Banda"),
            (DANADO, TITULO, b"Canci\xf3n"),
            (DESCONOCIDO, 9, b"raro"),
        ])
        self.assertEqual(salida, ["Artista: Banda"])
        self.assertEqual(errores, [
            "ADVERTENCIA: metadato desconocido (dañado): Canci?n",
            "ADVERTENCIA: metadato desconocido (categoría 9): raro",
        ])

    def test_sin_metadatos(self):
        self.assertEqual(n.notificar([]), ([], ["ADVERTENCIA: no se encontraron metadatos"]))

    def test_aviso_no_cabe(self):
        lineas = n.aviso_no_cabe(840, 417, 2112)
        self.assertIn("Se codificaron 417 de 840 bits; el resto se perdió.", lineas)
        self.assertIn("Para codificarlos completos faltan unos 20.3 s de audio.", lineas)

    def test_regla_de_los_signos_de_pregunta(self):
        casos = {
            "Ñandú".encode(): "Ñandú",
            b"a\xe2\x82": "a??",           # truncada: un ? por byte
            b"\xc0\xaf": "??",             # sobrelarga
            b"\xed\xa0\x80": "???",        # sustituto
            b"\xf4\x90\x80\x80": "????",   # mayor que U+10FFFF
            b"\xf0\x9f\x8e\xb5x": "\U0001F3B5x",
            b"\x80": "?",
        }
        for datos, esperado in casos.items():
            self.assertEqual(n.texto_visible(datos), esperado, datos)


if __name__ == "__main__":
    unittest.main()
