// Pruebas de src/notificacion.* (las mismas que prototipo/tests/test_notificacion.py)
// y comparación con los vectores de prototipo/vectores_notificacion.py. Los vectores
// de los canales prueban juntos trama_registros::leer, metadatos::desde_bits y
// notificar: de los bits recibidos al texto que se imprime.

#include <algorithm>
#include <clocale>
#include <cstddef>
#include <cstdint>
#include <locale>
#include <stdexcept>
#include <string>
#include <vector>

#include "metadatos.hpp"
#include "notificacion.hpp"
#include "parametros.hpp"
#include "pruebas.hpp"
#include "trama_de_prueba.hpp"

using namespace parametros;
using trama::Lectura;
using trama::Tipo;
namespace n = notificacion;

// ---- Comparación con Python -------------------------------------------------

PRUEBA(notificacion_canales_igual_que_python) {
    for (const char* canal : {"limpio", "inversion_1", "inversion_5", "borrado", "insercion"}) {
        const std::string base = std::string("notificacion_canal_") + canal;
        const auto enteros = pruebas::leer_enteros(base + "_bits");
        const trama::Bits bits(enteros.begin(), enteros.end());
        const auto [salida, errores] = n::notificar(metadatos::desde_bits(bits));
        VERIFICAR_IGUAL(salida, pruebas::leer_textos(base + "_salida"));
        VERIFICAR_IGUAL(errores, pruebas::leer_textos(base + "_errores"));
    }
}

PRUEBA(notificacion_visible_igual_que_python) {
    const auto entradas = pruebas::leer_textos("notificacion_visible_entradas");
    const auto salidas = pruebas::leer_textos("notificacion_visible_salidas");
    REQUERIR(!entradas.empty() && entradas.size() == salidas.size());
    for (std::size_t i = 0; i < entradas.size(); ++i)
        VERIFICAR_IGUAL(n::texto_visible(entradas[i]), salidas[i]);
}

PRUEBA(notificacion_aviso_no_cabe_igual_que_python) {
    // Los mismos argumentos que en prototipo/vectores_notificacion.py.
    const auto copia = static_cast<std::int64_t>(metadatos::bits_necesarios(SIETE));
    VERIFICAR_IGUAL(n::aviso_no_cabe(copia, 300, 2112),
                    pruebas::leer_textos("notificacion_aviso_no_cabe"));
}

// ---- Casos ------------------------------------------------------------------

PRUEBA(notificacion_lineas) {
    const auto [salida, errores] = n::notificar({
        {Tipo::valido, ARTISTA, "Banda"},
        {Tipo::danado, TITULO, "Canci\xf3n"},
        {Tipo::desconocido, 9, "raro"},
    });
    VERIFICAR_IGUAL(salida, std::vector<std::string>{"Artista: Banda"});
    VERIFICAR_IGUAL(errores, (std::vector<std::string>{
                                 "ADVERTENCIA: metadato desconocido (dañado): Canci?n",
                                 "ADVERTENCIA: metadato desconocido (categoría 9): raro",
                             }));
}

PRUEBA(notificacion_sin_metadatos) {
    const auto [salida, errores] = n::notificar({});
    VERIFICAR(salida.empty());
    VERIFICAR_IGUAL(errores,
                    std::vector<std::string>{"ADVERTENCIA: no se encontraron metadatos"});
}

PRUEBA(notificacion_aviso_no_cabe) {
    const auto lineas = n::aviso_no_cabe(840, 417, 2112);
    const auto contiene = [&](const std::string& l) {
        return std::find(lineas.begin(), lineas.end(), l) != lineas.end();
    };
    VERIFICAR(contiene("Se codificaron 417 de 840 bits; el resto se perdió."));
    VERIFICAR(contiene("Para codificarlos completos faltan unos 20.3 s de audio."));
}

PRUEBA(notificacion_valido_con_categoria_invalida_lanza) {
    bool lanzo = false;
    try {
        n::notificar({{Tipo::valido, 9, "x"}});
    } catch (const std::invalid_argument&) {
        lanzo = true;
    }
    VERIFICAR(lanzo);
}

PRUEBA(notificacion_aviso_no_depende_de_la_locale) {
    // Locale de C++ con coma decimal (sin depender de las locales instaladas) y, si la
    // máquina tiene alguna, locale de C con coma (la que usa printf).
    struct Coma : std::numpunct<char> {
        char do_decimal_point() const override { return ','; }
    };
    const std::locale anterior = std::locale::global(std::locale(std::locale::classic(), new Coma));
    const std::string anterior_c = std::setlocale(LC_NUMERIC, nullptr);
    for (const char* l : {"es_CR.UTF-8", "es_ES.UTF-8", "de_DE.UTF-8", "fr_FR.UTF-8"})
        if (std::setlocale(LC_NUMERIC, l) != nullptr) break;
    const auto lineas = n::aviso_no_cabe(840, 417, 2112);
    std::setlocale(LC_NUMERIC, anterior_c.c_str());
    std::locale::global(anterior);
    REQUERIR(lineas.size() == 5);
    VERIFICAR_IGUAL(lineas[3], "Para codificarlos completos faltan unos 20.3 s de audio.");
}

PRUEBA(notificacion_regla_de_los_signos_de_pregunta) {
    VERIFICAR_IGUAL(n::texto_visible("Ñandú"), "Ñandú");
    VERIFICAR_IGUAL(n::texto_visible("a\xe2\x82"), "a??");                 // truncada
    VERIFICAR_IGUAL(n::texto_visible("\xc0\xaf"), "??");                   // sobrelarga
    VERIFICAR_IGUAL(n::texto_visible("\xed\xa0\x80"), "???");              // sustituto
    VERIFICAR_IGUAL(n::texto_visible("\xf4\x90\x80\x80"), "????");         // > U+10FFFF
    VERIFICAR_IGUAL(n::texto_visible("\xf0\x9f\x8e\xb5x"), "\xf0\x9f\x8e\xb5x");
    VERIFICAR_IGUAL(n::texto_visible("\x80"), "?");
}
