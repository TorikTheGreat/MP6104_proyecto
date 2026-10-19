// Pruebas de src/linea_comandos.* (las mismas que prototipo/tests/test_linea_comandos.py)
// y comparación con los casos de prototipo/vectores_linea_comandos.py.

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <string>
#include <variant>
#include <vector>

#include "linea_comandos.hpp"
#include "parametros.hpp"
#include "pruebas.hpp"

using namespace parametros;
namespace lc = linea_comandos;

namespace {

using Resultado = std::variant<lc::Configuracion, lc::ErrorUso>;

Resultado analizar(const std::vector<std::string>& args) { return lc::analizar(args); }

bool es_error(const Resultado& r) { return std::holds_alternative<lc::ErrorUso>(r); }

// Copia de la configuración, o una vacía si hubo error (las pruebas que la usan
// revisan antes con REQUERIR o con el valor esperado).
lc::Configuracion config(const Resultado& r) {
    const auto* c = std::get_if<lc::Configuracion>(&r);
    return c ? *c : lc::Configuracion{};
}

// Copia del metadato de esa categoría (una copia: VERIFICAR_IGUAL guarda una
// referencia, y metadatos.at() de un temporal quedaría colgando).
std::string metadato(const Resultado& r, int categoria) {
    const auto c = config(r);
    const auto it = c.metadatos.find(categoria);
    return it == c.metadatos.end() ? "<sin metadato>" : it->second;
}

std::string real(double x) {
    char s[64];
    std::snprintf(s, sizeof s, "%.17g", x);
    return s;
}

// Las líneas que escribe _lineas() de prototipo/vectores_linea_comandos.py.
std::vector<std::string> lineas(const std::vector<std::string>& args) {
    std::vector<std::string> l = {"caso"};
    for (const auto& a : args) l.push_back("arg " + a);
    const Resultado r = lc::analizar(args);
    if (const auto* e = std::get_if<lc::ErrorUso>(&r)) {
        l.push_back("codigo=" + std::to_string(lc::ERROR_USO));
        l.push_back("mensaje=" + e->mensaje);
        return l;
    }
    const auto c = config(r);
    l.push_back("codigo=" + std::to_string(lc::EXITO));
    l.push_back(std::string("modo=") + lc::nombre_modo(c.modo));
    l.push_back("entrada=" + c.entrada);
    l.push_back("salida=" + c.salida);
    const char* campos[] = {"titulo", "artista", "autor", "album", "anio", "invitado", "productor"};
    for (int categoria = 1; categoria <= 7; ++categoria) {
        const auto dato = c.metadatos.find(categoria);
        const std::string campo = campos[categoria - 1];
        l.push_back(dato == c.metadatos.end() ? campo : campo + "=" + dato->second);
    }
    l.push_back(c.retardo0_us ? "retardo0=" + std::to_string(*c.retardo0_us) : "retardo0");
    l.push_back(c.retardo1_us ? "retardo1=" + std::to_string(*c.retardo1_us) : "retardo1");
    l.push_back(c.atenuacion0 ? "atenuacion0=" + real(*c.atenuacion0) : "atenuacion0");
    l.push_back(c.atenuacion1 ? "atenuacion1=" + real(*c.atenuacion1) : "atenuacion1");
    l.push_back("canal=" + c.canal);
    l.push_back(c.diagnostico ? "diagnostico=" + *c.diagnostico : "diagnostico");
    return l;
}

}  // namespace

// ---- Comparación con Python -------------------------------------------------

PRUEBA(linea_comandos_casos_igual_que_python) {
    const auto esperado = pruebas::leer_textos("linea_comandos_casos");
    // Separa los casos: cada uno empieza con "caso"; sus argumentos son las
    // líneas "arg ..." que siguen.
    std::size_t casos = 0;
    for (std::size_t i = 0; i < esperado.size();) {
        REQUERIR(esperado[i] == "caso");
        std::size_t fin = i + 1;
        while (fin < esperado.size() && esperado[fin] != "caso") ++fin;
        std::vector<std::string> args;
        for (std::size_t k = i + 1; k < fin && esperado[k].rfind("arg ", 0) == 0; ++k)
            args.push_back(esperado[k].substr(4));
        const std::vector<std::string> caso(esperado.begin() + static_cast<std::ptrdiff_t>(i),
                                            esperado.begin() + static_cast<std::ptrdiff_t>(fin));
        VERIFICAR_IGUAL(lineas(args), caso);
        ++casos;
        i = fin;
    }
    VERIFICAR(casos > 90);
}

PRUEBA(linea_comandos_uso_igual_que_python) {
    std::string esperado;
    for (const auto& l : pruebas::leer_textos("linea_comandos_uso")) esperado += l + "\n";
    REQUERIR(!esperado.empty());
    VERIFICAR_IGUAL(lc::uso(), esperado);
}

// ---- Modo -------------------------------------------------------------------

PRUEBA(linea_comandos_codificar_y_decodificar) {
    auto r = analizar({"e.wav", "s.wav", "--titulo", "Hola"});
    REQUERIR(!es_error(r));
    VERIFICAR(config(r).modo == lc::Modo::codificar);
    VERIFICAR_IGUAL(config(r).entrada, "e.wav");
    VERIFICAR_IGUAL(config(r).salida, "s.wav");
    VERIFICAR(config(r).metadatos == trama::Metadatos({{TITULO, "Hola"}}));
    r = analizar({"e.wav"});
    REQUERIR(!es_error(r));
    VERIFICAR(config(r).modo == lc::Modo::decodificar);
    VERIFICAR(config(r).metadatos.empty());
}

PRUEBA(linea_comandos_posicionales_equivocados) {
    VERIFICAR(es_error(analizar({})));
    VERIFICAR(es_error(analizar({"e.wav", "s.wav"})));
    VERIFICAR(es_error(analizar({"e.wav", "--titulo", "x"})));
    VERIFICAR(es_error(analizar({"a", "b", "c", "--titulo", "x"})));
}

PRUEBA(linea_comandos_salida_igual_a_entrada) {
    VERIFICAR(es_error(analizar({"e.wav", "e.wav", "--titulo", "x"})));
}

PRUEBA(linea_comandos_ayuda) {
    const auto r = analizar({"--ayuda"});
    REQUERIR(!es_error(r));
    VERIFICAR(config(r).modo == lc::Modo::ayuda);
    VERIFICAR(es_error(analizar({"--ayuda=x"})));
}

PRUEBA(linea_comandos_opciones_desconocidas) {
    VERIFICAR(es_error(analizar({"e.wav", "--x"})));
    VERIFICAR(es_error(analizar({"e.wav", "-h"})));
    VERIFICAR(es_error(analizar({"e.wav", "--titulos", "a"})));
}

// ---- Sintaxis ---------------------------------------------------------------

PRUEBA(linea_comandos_igual_y_separado) {
    VERIFICAR_IGUAL(metadato(analizar({"e", "s", "--titulo=a=b"}), TITULO), "a=b");
    VERIFICAR_IGUAL(metadato(analizar({"--titulo", "a", "e", "s"}), TITULO), "a");
}

PRUEBA(linea_comandos_ultima_gana) {  // R9
    VERIFICAR_IGUAL(metadato(analizar({"e", "s", "--titulo", "a", "--titulo", "b"}), TITULO),
                    "b");
    const auto r = analizar({"e", "--retardo0", "2000", "--retardo0", "1000", "--retardo1", "1500"});
    VERIFICAR(config(r).retardo0_us == 1000);
}

PRUEBA(linea_comandos_solo_se_valida_la_ultima_repeticion) {  // R9
    for (const std::string& primero : {std::string(""), std::string(300, 'x'), std::string("\xff")}) {
        const auto r = analizar({"e", "s", "--titulo", primero, "--titulo", "ok"});
        REQUERIR(!es_error(r));
        VERIFICAR_IGUAL(metadato(r, TITULO), "ok");
    }
    const auto r = analizar({"e", "--retardo0", "abc", "--retardo0", "1000", "--retardo1", "1500"});
    VERIFICAR(config(r).retardo0_us == 1000);
    VERIFICAR(es_error(analizar({"e", "s", "--titulo", "ok", "--titulo", ""})));
}

PRUEBA(linea_comandos_valor_que_parece_opcion) {
    VERIFICAR(es_error(analizar({"e", "s", "--titulo", "-x"})));
    VERIFICAR_IGUAL(metadato(analizar({"e", "s", "--titulo=-x"}), TITULO), "-x");
    VERIFICAR_IGUAL(metadato(analizar({"e", "s", "--titulo", "-"}), TITULO), "-");
}

PRUEBA(linea_comandos_doble_guion) {
    const auto r = analizar({"--titulo", "x", "--", "-raro.wav", "--s.wav"});
    REQUERIR(!es_error(r));
    VERIFICAR_IGUAL(config(r).entrada, "-raro.wav");
    VERIFICAR_IGUAL(config(r).salida, "--s.wav");
}

// ---- Valores ----------------------------------------------------------------

PRUEBA(linea_comandos_metadatos) {
    VERIFICAR(es_error(analizar({"e", "s", "--titulo", "inv\xe1lido"})));
    VERIFICAR(es_error(analizar({"e", "s", "--titulo", ""})));
    VERIFICAR(es_error(analizar({"e", "s", "--titulo", std::string(256, 'x')})));
    VERIFICAR(config(analizar({"e", "s", "--album", "Álbum"})).metadatos ==
              trama::Metadatos({{ALBUM, "Álbum"}}));
}

PRUEBA(linea_comandos_retardos) {
    const auto r = analizar({"e", "--retardo0", "12", "--retardo1", "1000000"});
    REQUERIR(!es_error(r));
    VERIFICAR(config(r).retardo0_us == 12);
    VERIFICAR(config(r).retardo1_us == 1000000);
    for (const char* malo : {"11", "0", "1000001", "1e3", " 12", "١٢", "12\n", "99999999999",
                             "+5", "0x10"})
        VERIFICAR(es_error(analizar({"e", "--retardo0", malo, "--retardo1", "1500"})));
}

PRUEBA(linea_comandos_retardos_con_las_mismas_muestras) {
    VERIFICAR(es_error(analizar({"e", "--retardo0", "1000", "--retardo1", "1005"})));
}

PRUEBA(linea_comandos_t0_igual_t1_con_valores_por_defecto) {
    // Solo retardo0, igual al por defecto de retardo1: se rechaza con el resuelto.
    VERIFICAR(es_error(analizar({"e", "--retardo0", std::to_string(RETARDO1_US)})));
}

PRUEBA(linea_comandos_atenuaciones) {
    const auto r = analizar({"e", "--atenuacion0", "12.5", "--atenuacion1", "99.999999"});
    REQUERIR(!es_error(r));
    VERIFICAR(config(r).atenuacion0 == 12.5);
    VERIFICAR(config(r).atenuacion1 == 99.999999);
    for (const std::string& malo : {std::string("0"), std::string("0.0"), std::string("100"),
                                   std::string("100.000001"), std::string("nan"),
                                   std::string("inf"), std::string("30."), std::string(".5"),
                                   std::string("1e1"), std::string("-3"),
                                   "1." + std::string(400, '1')})
        VERIFICAR(es_error(analizar({"e", "--atenuacion0", malo})));
}

PRUEBA(linea_comandos_atenuacion_igual_que_strtod) {
    // La conversión con enteros da el mismo double que convertir el texto.
    std::mt19937 rng(4);
    for (int k = 0; k < 20000; ++k) {
        const unsigned entero = rng() % 100;
        const unsigned cifras = 1 + rng() % 6;
        std::string decimales = std::to_string(rng() % 1000000);
        decimales = std::string(decimales.size() < cifras ? cifras - decimales.size() : 0, '0') +
                    decimales;
        decimales.resize(cifras);
        const std::string texto = std::to_string(entero) + "." + decimales;
        const double esperado = std::strtod(texto.c_str(), nullptr);
        if (!(esperado > 0.0)) continue;
        const auto r = analizar({"e", "--atenuacion0", texto});
        REQUERIR(!es_error(r));
        VERIFICAR(config(r).atenuacion0 == esperado);
    }
}

PRUEBA(linea_comandos_por_defecto_quedan_sin_dar) {
    const auto c = config(analizar({"e"}));
    VERIFICAR(!c.retardo0_us && !c.atenuacion0 && !c.diagnostico);
    VERIFICAR_IGUAL(c.canal, "izquierdo");
    const auto r = lc::resolver(c);
    VERIFICAR_IGUAL(r.retardo0_us, RETARDO0_US);
    VERIFICAR_IGUAL(r.retardo1_us, RETARDO1_US);
    VERIFICAR_IGUAL(r.atenuacion0, ATENUACION0);
    VERIFICAR_IGUAL(r.atenuacion1, ATENUACION1);
}

PRUEBA(linea_comandos_canal_y_diagnostico) {
    VERIFICAR_IGUAL(config(analizar({"e", "--canal", "derecho"})).canal, "derecho");
    VERIFICAR(es_error(analizar({"e", "--canal", "centro"})));
    VERIFICAR(config(analizar({"e", "--diagnostico", "d.csv"})).diagnostico == "d.csv");
    VERIFICAR(es_error(analizar({"e", "--diagnostico="})));
}

PRUEBA(linea_comandos_mensajes) {
    const auto mensaje = [](const Resultado& r) {
        const auto* e = std::get_if<lc::ErrorUso>(&r);
        return e ? e->mensaje : std::string("<sin error>");
    };
    VERIFICAR_IGUAL(mensaje(analizar({"e", "--canal", "centro"})),
                    "--canal: se esperaba izquierdo o derecho, no \"centro\"");
    VERIFICAR_IGUAL(mensaje(analizar({"e", "s", "--titulo", "\xff"})),
                    "--titulo: el texto no es UTF-8 válido");
    VERIFICAR_IGUAL(mensaje(analizar({"e", "--retardo0", "a\xff"})),
                    "--retardo0: se esperaba un entero de us, no \"a\\xff\"");
    VERIFICAR_IGUAL(mensaje(analizar({"e", "--x\xff"})), "opción desconocida: --x\\xff");
}
