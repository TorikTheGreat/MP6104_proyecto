// Pruebas de src/metadatos.* (las mismas que prototipo/tests/test_metadatos.py) y
// comparación con los vectores de prototipo/vectores_metadatos.py.

#include <cstddef>
#include <random>
#include <stdexcept>
#include <vector>

#include "metadatos.hpp"
#include "parametros.hpp"
#include "pruebas.hpp"
#include "trama_de_prueba.hpp"
#include "trama_registros.hpp"

using namespace parametros;
using trama::Bits;
using trama::Lectura;
using trama::Tipo;
namespace m = metadatos;
namespace f = trama_registros;

namespace {

trama::Metadatos validos(const std::vector<Lectura>& resultados) {
    trama::Metadatos v;
    for (const auto& l : resultados)
        if (l.tipo == Tipo::valido) v[l.categoria] = l.datos;
    return v;
}

Bits veces(const Bits& bits, int n) {
    Bits todo;
    for (int k = 0; k < n; ++k) todo.insert(todo.end(), bits.begin(), bits.end());
    return todo;
}

}  // namespace

// ---- Comparación con Python -------------------------------------------------

PRUEBA(metadatos_repetido_igual_que_python) {
    const std::size_t copia = m::bits_necesarios(SIETE);
    const auto r = m::a_bits(SIETE, 4 * copia + 77);
    VERIFICAR(!r.truncado);
    VERIFICAR_IGUAL(r.bits, pruebas::leer_enteros("metadatos_siete_repetido_bits"));
}

PRUEBA(metadatos_truncado_igual_que_python) {
    const auto r = m::a_bits(SIETE, 300);
    VERIFICAR(r.truncado);
    VERIFICAR_IGUAL(r.bits, pruebas::leer_enteros("metadatos_siete_truncado_bits"));
}

// ---- Codificar --------------------------------------------------------------

PRUEBA(metadatos_repetir) {
    VERIFICAR_IGUAL(m::repetir({1, 0, 1}, 7), Bits({1, 0, 1, 1, 0, 1, 1}));
    VERIFICAR(m::repetir({1, 0}, 0).empty());
    bool lanzo = false;
    try {
        m::repetir({}, 3);
    } catch (const std::invalid_argument&) {
        lanzo = true;
    }
    VERIFICAR(lanzo);
}

PRUEBA(metadatos_llena_las_ventanas_repitiendo) {
    const Bits copia = f::codificar({{TITULO, "Hola"}});
    const auto r = m::a_bits({{TITULO, "Hola"}}, 200);
    VERIFICAR_IGUAL(r.bits.size(), 200);
    VERIFICAR_IGUAL(r.necesarios, 72);
    VERIFICAR(!r.truncado);
    VERIFICAR_IGUAL(r.bits, m::repetir(copia, 200));
}

PRUEBA(metadatos_sin_repetir_va_una_vez) {
    const auto r = m::a_bits({{TITULO, "Hola"}}, 200, SiNoCabe::truncar, false);
    VERIFICAR_IGUAL(r.bits, f::codificar({{TITULO, "Hola"}}));
}

PRUEBA(metadatos_no_cabe_trunca) {
    const auto r = m::a_bits({{TITULO, "Hola"}}, 50, SiNoCabe::truncar);
    VERIFICAR_IGUAL(r.bits.size(), 50);
    VERIFICAR_IGUAL(r.necesarios, 72);
    VERIFICAR(r.truncado);
}

PRUEBA(metadatos_no_cabe_con_error) {
    bool lanzo = false;
    try {
        m::a_bits({{TITULO, "Hola"}}, 50, SiNoCabe::error);
    } catch (const m::NoCabe& e) {
        lanzo = true;
        VERIFICAR_IGUAL(e.necesarios, 72);
        VERIFICAR_IGUAL(e.disponibles, 50);
    }
    VERIFICAR(lanzo);
}

PRUEBA(metadatos_bits_necesarios) { VERIFICAR_IGUAL(m::bits_necesarios({{TITULO, "Hola"}}), 72); }

// ---- Desde bits -------------------------------------------------------------

PRUEBA(metadatos_ida_y_vuelta) {
    const Bits bits = m::a_bits(SIETE, 3 * m::bits_necesarios(SIETE) + 50).bits;
    VERIFICAR(validos(m::desde_bits(bits)) == SIETE);
}

PRUEBA(metadatos_texto_mas_frecuente_gana) {
    const Bits bits = unir({registro(TITULO, "malo"), veces(registro(TITULO, "bueno"), 2)});
    const std::vector<Lectura> esperado = {{Tipo::valido, TITULO, "bueno"}};
    VERIFICAR(m::desde_bits(bits) == esperado);
}

PRUEBA(metadatos_empate_gana_el_primero) {
    const Bits bits = unir({registro(TITULO, "uno"), registro(TITULO, "dos")});
    const std::vector<Lectura> esperado = {{Tipo::valido, TITULO, "uno"}};
    VERIFICAR(m::desde_bits(bits) == esperado);
}

PRUEBA(metadatos_danado_se_avisa_aunque_aparezca_una_vez) {  // R50, sección 4.3.4
    const Bits bits = unir({registro(ARTISTA, "Banda"), registro(TITULO, "Canción", 0)});
    const std::vector<Lectura> esperado = {{Tipo::valido, ARTISTA, "Banda"},
                                           {Tipo::danado, TITULO, "Canción"}};
    VERIFICAR(m::desde_bits(bits) == esperado);
}

PRUEBA(metadatos_danado_de_una_categoria_ya_valida_no_se_avisa) {
    const Bits bits = unir({registro(TITULO, "bien"), registro(TITULO, "mal", 0)});
    const std::vector<Lectura> esperado = {{Tipo::valido, TITULO, "bien"}};
    VERIFICAR(m::desde_bits(bits) == esperado);
}

PRUEBA(metadatos_danados_iguales_se_avisan_una_vez) {
    const Bits bits = veces(unir({registro(ARTISTA, "Banda"), registro(TITULO, "x", 0)}), 3);
    const std::vector<Lectura> esperado = {{Tipo::valido, ARTISTA, "Banda"},
                                           {Tipo::danado, TITULO, "x"}};
    VERIFICAR(m::desde_bits(bits) == esperado);
}

PRUEBA(metadatos_danados_con_el_mismo_texto_se_avisan_una_vez) {
    // El byte de categoría de un dañado no es confiable: se agrupan por texto.
    const Bits bueno = registro(AUTOR, "ok");
    const Bits bits = unir({bueno, registro(TITULO, "x", 0), bueno, registro(ARTISTA, "x", 0)});
    const std::vector<Lectura> esperado = {{Tipo::valido, AUTOR, "ok"},
                                           {Tipo::danado, TITULO, "x"}};
    VERIFICAR(m::desde_bits(bits) == esperado);
}

PRUEBA(metadatos_desconocidos) {
    const Bits bits = unir({registro(TITULO, "a"), registro(9, "raro"), registro(9, "raro")});
    const std::vector<Lectura> esperado = {{Tipo::valido, TITULO, "a"},
                                           {Tipo::desconocido, 9, "raro"}};
    VERIFICAR(m::desde_bits(bits) == esperado);
}

PRUEBA(metadatos_sin_ningun_valido_no_se_avisa) {
    const Bits bits = unir({registro(9, "raro"), registro(TITULO, "x", 0)});
    VERIFICAR(m::desde_bits(bits).empty());
}

PRUEBA(metadatos_sin_ningun_valido_se_avisa_si_se_configura) {
    const Bits bits = unir({registro(9, "raro"), registro(TITULO, "x", 0)});
    const auto r = m::desde_bits(bits, false);
    REQUERIR(r.size() == 2);
    VERIFICAR(r[0].tipo == Tipo::desconocido);
    VERIFICAR(r[1].tipo == Tipo::danado);
    const std::vector<Lectura> esperado = {{Tipo::danado, TITULO, "x"}};
    VERIFICAR(m::desde_bits(registro(TITULO, "x", 0), false) == esperado);
}

PRUEBA(metadatos_bits_aleatorios_no_producen_nada) {
    std::mt19937 rng(3);
    for (int k = 0; k < 20; ++k) {
        Bits bits(1000);
        for (int& b : bits) b = static_cast<int>(rng() & 1);
        VERIFICAR(m::desde_bits(bits).empty());
    }
}
