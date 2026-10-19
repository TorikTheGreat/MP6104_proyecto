// Dueño: B.
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include "decodificador.hpp"
#include "parametros.hpp"
#include "pruebas.hpp"

namespace {

double error_max(const std::vector<double>& a, const std::vector<double>& b) {
    double e = 0.0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        e = std::max(e, std::abs(a[i] - b[i]));
    }
    return e;
}

}  // namespace

PRUEBA(decodificador_cepstro_igual_que_python) {
    for (int b : {0, 1}) {
        const std::string base = "decodificador_ventana_" + std::to_string(b) + "_";
        const auto entrada = pruebas::leer_reales(base + "entrada");
        const auto esperado = pruebas::leer_reales(base + "rcc");
        const auto obtenido = autocorrelacion_cepstro(entrada);
        REQUERIR(obtenido.size() == esperado.size());
        VERIFICAR_CERCA(error_max(obtenido, esperado), 0.0, 1e-11);

        const auto d0 = static_cast<std::size_t>(
            parametros::muestras_desde_us(parametros::RETARDO0_US));
        const auto d1 = static_cast<std::size_t>(
            parametros::muestras_desde_us(parametros::RETARDO1_US));
        VERIFICAR_IGUAL(clasificar_ventana(entrada, d0, d1), b);
    }
}

PRUEBA(decodificador_senal_igual_que_python) {
    const auto entrada_i64 = pruebas::leer_enteros("decodificador_senal_entrada");
    std::vector<std::int16_t> entrada(entrada_i64.size());
    for (std::size_t i = 0; i < entrada_i64.size(); ++i) {
        entrada[i] = static_cast<std::int16_t>(entrada_i64[i]);
    }

    const auto bits_esperados_i64 = pruebas::leer_enteros("decodificador_senal_bits_esperados");
    const auto bits_ventanas_i64 = pruebas::leer_enteros("decodificador_senal_bits_ventanas");
    const auto r0_esperado = pruebas::leer_reales("decodificador_senal_r0");
    const auto r1_esperado = pruebas::leer_reales("decodificador_senal_r1");

    const auto res = decodificar_por_us(entrada);
    REQUERIR(res.bits.size() == bits_esperados_i64.size());
    for (std::size_t i = 0; i < res.bits.size(); ++i) {
        VERIFICAR_IGUAL(res.bits[i], bits_esperados_i64[i]);
    }

    REQUERIR(res.ventanas.size() == bits_ventanas_i64.size());
    for (std::size_t k = 0; k < res.ventanas.size(); ++k) {
        VERIFICAR_IGUAL(res.ventanas[k].bit, bits_ventanas_i64[k]);
        VERIFICAR_CERCA(res.ventanas[k].r0, r0_esperado[k], 1e-11);
        VERIFICAR_CERCA(res.ventanas[k].r1, r1_esperado[k], 1e-11);
    }
}

PRUEBA(decodificador_invarianza_de_escala) {
    const auto entrada = pruebas::leer_reales("decodificador_ventana_1_entrada");
    const auto rcc_base = autocorrelacion_cepstro(entrada);
    const auto d0 =
        static_cast<std::size_t>(parametros::muestras_desde_us(parametros::RETARDO0_US));
    const auto d1 =
        static_cast<std::size_t>(parametros::muestras_desde_us(parametros::RETARDO1_US));

    for (double escala : {1e-2, 1.0, 250.0, 10000.0}) {
        std::vector<double> escalada(entrada.size());
        for (std::size_t i = 0; i < entrada.size(); ++i) {
            escalada[i] = entrada[i] * escala;
        }
        const auto rcc_esc = autocorrelacion_cepstro(escalada);
        REQUERIR(rcc_esc.size() == rcc_base.size());
        VERIFICAR_CERCA(error_max(rcc_esc, rcc_base), 0.0, 1e-11);
        VERIFICAR_IGUAL(clasificar_ventana(escalada, d0, d1), 1);
    }
}

PRUEBA(decodificador_reconoce_silencio_y_parametros_invalidos) {
    const std::vector<double> silencio(1650, 0.0);
    VERIFICAR_IGUAL(clasificar_ventana(silencio, 44, 66), SIN_ECO);

    bool lanzo = false;
    try {
        clasificar_ventana( std::vector<double>{}, 44, 66);
    } catch (const std::invalid_argument&) {
        lanzo = true;
    }
    VERIFICAR(lanzo);

    lanzo = false;
    try {
        clasificar_ventana(silencio, 44, 44);
    } catch (const std::invalid_argument&) {
        lanzo = true;
    }
    VERIFICAR(lanzo);

    lanzo = false;
    try {
        clasificar_ventana(silencio, 44, 2000);
    } catch (const std::invalid_argument&) {
        lanzo = true;
    }
    VERIFICAR(lanzo);
}
