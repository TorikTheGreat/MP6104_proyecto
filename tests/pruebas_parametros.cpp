// Pruebas de src/parametros.hpp, incluida la comparación con
// prototipo/vectores_parametros.py.

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

#include "parametros.hpp"
#include "pruebas.hpp"

using namespace parametros;

PRUEBA(parametros_retardos_bordes) {
    VERIFICAR_IGUAL(muestras_desde_us(11), 0);
    VERIFICAR_IGUAL(muestras_desde_us(12), 1);
    VERIFICAR_IGUAL(muestras_desde_us(34), 1);
    VERIFICAR_IGUAL(muestras_desde_us(1000), 44);
    VERIFICAR_IGUAL(muestras_desde_us(5000), 221);   // 220.5
    VERIFICAR_IGUAL(muestras_desde_us(25000), 1103);  // 1102.5
    VERIFICAR_IGUAL(muestras_desde_us(1000000), 44100);
}

PRUEBA(parametros_retardos_fuera_de_rango_lanzan) {
    for (std::int64_t malo : {std::int64_t{0}, std::int64_t{-1}, std::int64_t{1000001}}) {
        bool lanzo = false;
        try {
            muestras_desde_us(malo);
        } catch (const std::out_of_range&) {
            lanzo = true;
        }
        VERIFICAR(lanzo);
    }
}

static_assert(muestras_desde_us(1000) == 44, "muestras_desde_us debe servir en constexpr");
static_assert(largo_ventana(1000, 1500) == K_VENTANA * 66, "largo_ventana en constexpr");

PRUEBA(parametros_retardos_otros_tipos_enteros) {
    VERIFICAR_IGUAL(muestras_desde_us(static_cast<std::int8_t>(100)), 4);
    VERIFICAR_IGUAL(muestras_desde_us(1000u), 44);
    VERIFICAR_IGUAL(muestras_desde_us(std::size_t{1000000}), 44100);
    VERIFICAR_IGUAL(muestras_desde_us(RETARDO_MIN_UTIL_US), 1);
    VERIFICAR_IGUAL(muestras_desde_us(RETARDO_MIN_UTIL_US - 1), 0);
    bool lanzo = false;
    try {
        muestras_desde_us(std::uint64_t{1} << 40);
    } catch (const std::out_of_range&) {
        lanzo = true;
    }
    VERIFICAR(lanzo);
}

PRUEBA(parametros_a_int16_rechaza_no_finitos) {
    for (double malo : {std::nan(""), HUGE_VAL, -HUGE_VAL}) {
        bool lanzo = false;
        try {
            a_int16(malo);
        } catch (const std::domain_error&) {
            lanzo = true;
        }
        VERIFICAR(lanzo);
    }
}

PRUEBA(parametros_amplitud) {
    VERIFICAR_IGUAL(amplitud_desde_porcentaje(30.0), 0.3);
    VERIFICAR_IGUAL(amplitud_desde_porcentaje(50.0), 0.5);
    bool lanzo = false;
    try {
        amplitud_desde_porcentaje(0.0);
    } catch (const std::out_of_range&) {
        lanzo = true;
    }
    VERIFICAR(lanzo);
    lanzo = false;
    try {
        amplitud_desde_porcentaje(100.0);  // el codificador acepta (0, 100)
    } catch (const std::out_of_range&) {
        lanzo = true;
    }
    VERIFICAR(lanzo);
}

PRUEBA(parametros_escala_saturacion_y_redondeo) {
    VERIFICAR_IGUAL(a_int16(1.0), 32767);
    VERIFICAR_IGUAL(a_int16(1.5), 32767);
    VERIFICAR_IGUAL(a_int16(-1.0), -32768);
    VERIFICAR_IGUAL(a_int16(-1.5), -32768);
    VERIFICAR_IGUAL(a_int16(0.9 / 32768), 0);  // trunca hacia cero
    VERIFICAR_IGUAL(a_int16(-0.9 / 32768), 0);
    VERIFICAR_IGUAL(a_int16(-1.9 / 32768), -1);
    for (int m = -32768; m <= 32767; ++m)
        if (a_int16(a_flotante(static_cast<std::int16_t>(m))) != m) {
            VERIFICAR_IGUAL(a_int16(a_flotante(static_cast<std::int16_t>(m))), m);
            break;
        }
}

PRUEBA(parametros_ventana_y_valores_por_defecto) {
    VERIFICAR_IGUAL(largo_ventana(1000, 1500), K_VENTANA * 66);
    VERIFICAR_IGUAL(largo_ventana(1500, 1000), K_VENTANA * 66);
    VERIFICAR(muestras_desde_us(RETARDO0_US) != muestras_desde_us(RETARDO1_US));
    VERIFICAR_IGUAL(nombre_categoria(ALBUM), "Álbum");
    VERIFICAR(nombre_categoria(0) == nullptr);
    VERIFICAR(nombre_categoria(8) == nullptr);
}

PRUEBA(parametros_igual_que_python) {
    const auto t_us = pruebas::leer_enteros("parametros_retardos_us");
    const auto muestras = pruebas::leer_enteros("parametros_retardos_muestras");
    REQUERIR(t_us.size() == muestras.size());
    for (std::size_t i = 0; i < t_us.size(); ++i)
        VERIFICAR_IGUAL(muestras_desde_us(t_us[i]), muestras[i]);

    const auto y = pruebas::leer_reales("parametros_flotantes");
    const auto m = pruebas::leer_enteros("parametros_int16");
    REQUERIR(y.size() == m.size());
    for (std::size_t i = 0; i < y.size(); ++i) VERIFICAR_IGUAL(a_int16(y[i]), m[i]);
}
