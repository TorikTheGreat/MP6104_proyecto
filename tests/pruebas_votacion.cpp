/* 
Pruebas de src/votacion.* (las mismas que prototipo/tests/test_votacion.py) y
comparación con los vectores de prototipo/vectores_votacion.py.

Nota: algunas pruebas que se hicieron en Python no tienen equivalencia aquí ya que
los números aleatorios se generan con diferentes métodos en ambos lenguajes y las
semillas, aunque iguales, no producirían los mismos resultados.
*/
#include <cstddef>
#include <random>
#include <string>
#include <vector>

#include "metadatos.hpp"
#include "pruebas.hpp"
#include "trama_de_prueba.hpp"
#include "trama_registros.hpp"
#include "votacion.hpp"

using trama::Bits;
namespace v = votacion;

namespace {

const Bits COPIA = trama_registros::codificar(SIETE);

Bits veces(const Bits& bits, int n) {
    Bits todo;
    for (int k = 0; k < n; ++k) todo.insert(todo.end(), bits.begin(), bits.end());
    return todo;
}

bool es_rotacion(const Bits& a, const Bits& b) {
    if (a.size() != b.size()) return false;
    for (std::size_t k = 0; k < b.size(); ++k) {
        bool igual = true;
        for (std::size_t i = 0; i < b.size() && igual; ++i) igual = a[i] == b[(i + k) % b.size()];
        if (igual) return true;
    }
    return false;
}

}

// ---- Comparación con Python -------------------------------------------------

PRUEBA(votacion_igual_que_python) {
    std::size_t casos = 0;
    for (const char* caso : {"inversion_5", "borrado", "insercion", "cortada", "aleatorio"}) {
        const std::string base = std::string("votacion_") + caso;
        const auto entrada = pruebas::leer_enteros(base + "_entrada");
        const Bits bits(entrada.begin(), entrada.end());
        const auto periodo = pruebas::leer_enteros(base + "_periodo");
        const auto votada = pruebas::leer_enteros(base + "_votada");
        REQUERIR(!bits.empty() && periodo.size() == 1);
        VERIFICAR_IGUAL(v::estimar_periodo(bits).value_or(0), periodo[0]);
        VERIFICAR_IGUAL(v::votar_copias(bits).value_or(Bits{}), votada);
        ++casos;
    }
    VERIFICAR_IGUAL(casos, 5);
}

// ---- Período ----------------------------------------------------------------

PRUEBA(votacion_periodo_de_una_secuencia_limpia) {
    VERIFICAR(v::estimar_periodo(veces(COPIA, 4)) == COPIA.size());
}

PRUEBA(votacion_prefiere_el_menor_periodo) {
    const Bits corta = trama_registros::codificar({{parametros::TITULO, "ab"}});
    VERIFICAR(v::estimar_periodo(veces(corta, 12)) == corta.size());
}

PRUEBA(votacion_sin_tres_copias_no_hay_periodo) {
    Bits bits = veces(COPIA, 2);
    bits.insert(bits.end(), COPIA.begin(), COPIA.begin() + 500);
    VERIFICAR(!v::estimar_periodo(bits));
}

PRUEBA(votacion_bits_aleatorios_no_tienen_periodo) {
    std::mt19937 rng(3);
    for (int k = 0; k < 5; ++k) {
        Bits bits(3000);
        for (int& b : bits) b = static_cast<int>(rng() & 1);
        VERIFICAR(!v::estimar_periodo(bits));
    }
}

// ---- Voto -------------------------------------------------------------------

PRUEBA(votacion_limpia) { VERIFICAR(v::votar_copias(veces(COPIA, 3)) == COPIA); }

PRUEBA(votacion_un_bit_borrado_corre_solo_su_copia) {
    Bits bits = veces(COPIA, 5);
    bits.erase(bits.begin() + static_cast<std::ptrdiff_t>(COPIA.size() + 300));
    const auto votada = v::votar_copias(bits);
    REQUERIR(votada.has_value());
    VERIFICAR(es_rotacion(*votada, COPIA));
}

PRUEBA(votacion_empate_gana_la_primera_copia) {
    std::vector<Bits> copias(4, COPIA);
    copias[0][5] = copias[1][5] = 1 - COPIA[5];  // 2 contra 2
    Bits bits;
    for (const auto& c : copias) bits.insert(bits.end(), c.begin(), c.end());
    const auto votada = v::votar_copias(bits);
    REQUERIR(votada.has_value());
    VERIFICAR_IGUAL((*votada)[5], 1 - COPIA[5]);
}

PRUEBA(votacion_gana_la_mayoria_aunque_la_primera_copia_este_mal) {
    std::vector<Bits> copias(4, COPIA);
    copias[0][7] = 1 - COPIA[7];  // 1 contra 3
    Bits bits;
    for (const auto& c : copias) bits.insert(bits.end(), c.begin(), c.end());
    VERIFICAR(v::votar_copias(bits) == COPIA);
}

PRUEBA(votacion_la_copia_votada_cuenta_una_vez) {
    VERIFICAR_IGUAL(metadatos::leer_votada(veces(COPIA, 3)).size(), SIETE.size());
}

PRUEBA(votacion_se_puede_apagar) {
    // Con la votación apagada, desde_bits solo lee la secuencia tal cual.
    const auto entrada = pruebas::leer_enteros("votacion_inversion_5_entrada");
    const Bits bits(entrada.begin(), entrada.end());
    const auto con = metadatos::desde_bits(bits);
    const auto sin = metadatos::desde_bits(bits, parametros::AVISOS_REQUIEREN_VALIDO, false);
    std::size_t validos_con = 0, validos_sin = 0;
    for (const auto& l : con) validos_con += l.tipo == trama::Tipo::valido;
    for (const auto& l : sin) validos_sin += l.tipo == trama::Tipo::valido;
    VERIFICAR(validos_con > validos_sin);
}
