# Dueño: B.
"""Pruebas unitarias del decodificador en Python (prototipo/decodificador.py)."""

import unittest

import numpy as np

import decodificador as dec
import metadatos
import parametros as p


def _codificar_solapamiento_y_suma(
    x: np.ndarray, bits: list[int], d0: int, d1: int, a0: float, a1: float, largo: int
) -> np.ndarray:
    """Aplica H_b(z) = 1 + a_b z^(-d_b) por ventana con solapamiento y suma."""
    y = x.astype(np.float64).copy()
    for k, b in enumerate(bits):
        d = d0 if b == 0 else d1
        a = a0 if b == 0 else a1
        ini = k * largo
        fin = ini + largo
        if fin > x.size:
            break
        dest_fin = min(y.size, fin + d)
        n_eco = dest_fin - (ini + d)
        if n_eco > 0:
            y[ini + d : dest_fin] += a * x[ini : ini + n_eco]
    return y


class PruebasCepstroYClasificador(unittest.TestCase):
    def test_pico_en_retardo_correcto(self) -> None:
        rng = np.random.default_rng(44100)
        d0, d1 = 44, 66
        largo = 25 * d1
        x = rng.standard_normal(largo) * 4000.0

        y0 = x.copy()
        y0[d0:] += 0.3 * x[:-d0]
        rcc0 = dec.autocorrelacion_cepstro(y0)
        self.assertGreater(rcc0[d0], rcc0[d1] * 2.0)
        self.assertEqual(dec.clasificar_ventana(y0, d0, d1), 0)

        y1 = x.copy()
        y1[d1:] += 0.3 * x[:-d1]
        rcc1 = dec.autocorrelacion_cepstro(y1)
        self.assertGreater(rcc1[d1], rcc1[d0] * 2.0)
        self.assertEqual(dec.clasificar_ventana(y1, d0, d1), 1)

    def test_invarianza_de_escala_senales_fuertes_y_debiles(self) -> None:
        rng = np.random.default_rng(101)
        d0, d1 = 44, 66
        largo = 25 * d1
        base = rng.standard_normal(largo)
        con_eco = base.copy()
        con_eco[d1:] += 0.3 * base[:-d1]

        rcc_ref = dec.autocorrelacion_cepstro(con_eco)
        for escala in (1e-3, 0.02, 1.0, 500.0, 20000.0):
            rcc_esc = dec.autocorrelacion_cepstro(con_eco * escala)
            np.testing.assert_allclose(rcc_esc, rcc_ref, rtol=1e-12, atol=1e-12)
            self.assertEqual(dec.clasificar_ventana(con_eco * escala, d0, d1), 1)

    def test_ventana_silenciosa_o_sin_eco_devuelve_sin_eco(self) -> None:
        d0, d1 = 44, 66
        largo = 25 * d1
        silencio = np.zeros(largo)
        self.assertEqual(dec.clasificar_ventana(silencio, d0, d1), dec.SIN_ECO)

        rng = np.random.default_rng(2026)
        sin_eco_detectados = 0
        total = 50
        for _ in range(total):
            ruido = rng.standard_normal(largo) * 3000.0
            if dec.clasificar_ventana(ruido, d0, d1) == dec.SIN_ECO:
                sin_eco_detectados += 1
        self.assertGreaterEqual(sin_eco_detectados, int(0.85 * total))

    def test_validaciones_de_parametros(self) -> None:
        v = np.ones(200)
        with self.assertRaises(ValueError):
            dec.clasificar_ventana([], 10, 20)
        with self.assertRaises(ValueError):
            dec.clasificar_ventana(v, 0, 20)
        with self.assertRaises(ValueError):
            dec.clasificar_ventana(v, 15, 15)
        with self.assertRaises(ValueError):
            dec.clasificar_ventana(v, 10, 200)
        with self.assertRaises(ValueError):
            dec.rms_ventana(np.array([1.0, np.nan, 2.0]))


class PruebasDecodificarSenal(unittest.TestCase):
    def test_tasa_recuperacion_supera_90_por_ciento_con_valores_por_defecto(self) -> None:
        rng = np.random.default_rng(777)
        d0 = p.muestras_desde_us(p.RETARDO0_US)
        d1 = p.muestras_desde_us(p.RETARDO1_US)
        largo = p.largo_ventana(p.RETARDO0_US, p.RETARDO1_US)
        a0 = p.amplitud_desde_porcentaje(p.ATENUACION0)
        a1 = p.amplitud_desde_porcentaje(p.ATENUACION1)

        n_ventanas = 120
        x = rng.standard_normal(n_ventanas * largo) * 6000.0
        bits_ref = rng.integers(0, 2, size=n_ventanas).tolist()
        y = _codificar_solapamiento_y_suma(x, bits_ref, d0, d1, a0, a1, largo)
        y_int16 = np.clip(y, -32768, 32767).astype(np.int16)

        res = dec.decodificar_por_us(y_int16)
        self.assertEqual(len(res.bits), n_ventanas)
        aciertos = sum(1 for a, b in zip(bits_ref, res.bits) if a == b)
        self.assertGreaterEqual(aciertos / n_ventanas, 0.95)

    def test_senal_sin_codificar_no_produce_bits_ni_metadatos(self) -> None:
        rng = np.random.default_rng(999)
        largo = p.largo_ventana(p.RETARDO0_US, p.RETARDO1_US)
        x = (rng.standard_normal(60 * largo) * 5000.0).astype(np.int16)
        res = dec.decodificar_por_us(x)
        self.assertEqual(res.bits, [])
        self.assertEqual(metadatos.desde_bits(res.bits), [])

    def test_cola_sin_eco_se_recorta(self) -> None:
        rng = np.random.default_rng(1234)
        d0 = p.muestras_desde_us(p.RETARDO0_US)
        d1 = p.muestras_desde_us(p.RETARDO1_US)
        largo = p.largo_ventana(p.RETARDO0_US, p.RETARDO1_US)
        n_total = 40
        bits_prefijo = [1, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 1]
        x = rng.standard_normal(n_total * largo) * 5000.0
        y = _codificar_solapamiento_y_suma(x, bits_prefijo, d0, d1, 0.3, 0.3, largo)
        # Dejar la mitad final completamente silenciosa (sin eco).
        y[len(bits_prefijo) * largo :] = 0.0
        res = dec.decodificar_senal(y, d0, d1, largo_ventana=largo)
        self.assertEqual(res.bits, bits_prefijo)

    def test_ida_y_vuelta_siete_categorias_con_tildes(self) -> None:
        rng = np.random.default_rng(202610)
        d0 = p.muestras_desde_us(p.RETARDO0_US)
        d1 = p.muestras_desde_us(p.RETARDO1_US)
        largo = p.largo_ventana(p.RETARDO0_US, p.RETARDO1_US)
        a0 = p.amplitud_desde_porcentaje(p.ATENUACION0)
        a1 = p.amplitud_desde_porcentaje(p.ATENUACION1)

        originales = {
            p.TITULO: "Canción de otoño",
            p.ARTISTA: "Dúo Sinfónico",
            p.AUTOR: "José María Aguilar",
            p.ALBUM: "Música de Cámara",
            p.ANIO: "2026",
            p.INVITADO: "Cuarteto Clásico",
            p.PRODUCTOR: "Estudio Acústico TEC",
        }
        n_ventanas = 1800
        cod = metadatos.a_bits(originales, n_ventanas)
        self.assertFalse(cod.truncado)

        x = rng.standard_normal(n_ventanas * largo) * 5000.0
        y = _codificar_solapamiento_y_suma(x, cod.bits, d0, d1, a0, a1, largo)
        y_int16 = np.clip(y, -32768, 32767).astype(np.int16)

        res = dec.decodificar_por_us(y_int16)
        lecturas = metadatos.desde_bits(res.bits)
        recuperados = {cat: datos.decode("utf-8") for tipo, cat, datos in lecturas if tipo == "valido"}
        self.assertEqual(recuperados, originales)


if __name__ == "__main__":
    unittest.main()
