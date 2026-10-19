#include "linea_comandos.hpp"

#include <cstddef>
#include <cstdio>
#include <locale>
#include <map>
#include <sstream>

#include "parametros.hpp"
#include "utf8.hpp"

namespace linea_comandos {

namespace p = parametros;

namespace {

const std::map<std::string, int> METADATOS = {
    {"--titulo", p::TITULO},     {"--artista", p::ARTISTA}, {"--autor", p::AUTOR},
    {"--album", p::ALBUM},       {"--anio", p::ANIO},       {"--invitado", p::INVITADO},
    {"--productor", p::PRODUCTOR},
};

// Orden fijo en que se validan las opciones: el mismo primer error que en Python.
const std::vector<std::string> ORDEN = {
    "--titulo",    "--artista",   "--autor",       "--album",       "--anio",
    "--invitado",  "--productor", "--retardo0",    "--retardo1",    "--atenuacion0",
    "--atenuacion1", "--canal",   "--diagnostico",
};

bool con_valor(const std::string& nombre) {
    for (const auto& o : ORDEN)
        if (o == nombre) return true;
    return false;
}

// Como decode("utf-8", errors="backslashreplace") de Python: cada byte inválido
// se muestra como \xNN.
std::string mostrar(const std::string& b) {
    std::string texto;
    for (std::size_t i = 0; i < b.size();) {
        const std::size_t largo = utf8::largo_secuencia(b, i);
        if (largo > 0) {
            texto += b.substr(i, largo);
            i += largo;
        } else {
            char escape[8];
            std::snprintf(escape, sizeof escape, "\\x%02x", static_cast<unsigned char>(b[i]));
            texto += escape;
            i += 1;
        }
    }
    return texto;
}

bool es_opcion(const std::string& a) { return !a.empty() && a[0] == '-' && a != "-"; }

bool digitos(const std::string& s, std::size_t desde, std::size_t hasta) {
    for (std::size_t i = desde; i < hasta; ++i)
        if (s[i] < '0' || s[i] > '9') return false;
    return true;
}

// [0-9]{1,7}
bool forma_retardo(const std::string& v) {
    return !v.empty() && v.size() <= 7 && digitos(v, 0, v.size());
}

// [0-9]{1,3}(\.[0-9]{1,6})?
bool forma_atenuacion(const std::string& v) {
    const std::size_t punto = v.find('.');
    const std::size_t largo_entero = punto == std::string::npos ? v.size() : punto;
    if (largo_entero < 1 || largo_entero > 3 || !digitos(v, 0, largo_entero)) return false;
    if (punto == std::string::npos) return true;
    const std::size_t largo_decimales = v.size() - punto - 1;
    return largo_decimales >= 1 && largo_decimales <= 6 && digitos(v, punto + 1, v.size());
}

// Valida y guarda una opción; devuelve el mensaje de error o vacío.
std::string asignar(Configuracion& c, const std::string& nombre, const std::string& valor) {
    const std::string texto = mostrar(nombre);
    const auto metadato = METADATOS.find(nombre);
    if (metadato != METADATOS.end()) {
        if (!utf8::valido(valor)) return texto + ": el texto no es UTF-8 válido";
        if (valor.empty() || valor.size() > 255)
            return texto + ": el texto debe tener de 1 a 255 bytes (tiene " +
                   std::to_string(valor.size()) + ")";
        c.metadatos[metadato->second] = valor;
    } else if (nombre == "--retardo0" || nombre == "--retardo1") {
        if (!forma_retardo(valor))
            return texto + ": se esperaba un entero de us, no \"" + mostrar(valor) + "\"";
        const std::int64_t t = std::stoll(valor);
        if (t < p::RETARDO_MIN_UTIL_US || t > p::RETARDO_MAX_US)
            return texto + ": " + std::to_string(t) + " us está fuera de rango (" +
                   std::to_string(p::RETARDO_MIN_UTIL_US) + " a " +
                   std::to_string(p::RETARDO_MAX_US) + " us, al menos 1 muestra)";
        (nombre == "--retardo0" ? c.retardo0_us : c.retardo1_us) = t;
    } else if (nombre == "--atenuacion0" || nombre == "--atenuacion1") {
        if (!forma_atenuacion(valor))
            return texto + ": se esperaba un porcentaje como 30 o 12.5, no \"" + mostrar(valor) +
                   "\"";
        // Con enteros, para dar el mismo double que Python: millonésimas / 1e6.
        const std::size_t punto = valor.find('.');
        std::string decimales = punto == std::string::npos ? "" : valor.substr(punto + 1);
        decimales.resize(6, '0');
        const std::int64_t millonesimas =
            std::stoll(valor.substr(0, punto)) * 1000000 + std::stoll(decimales);
        if (millonesimas <= 0 || millonesimas >= 100000000)
            return texto + ": " + mostrar(valor) + " % está fuera de (0, 100)";
        (nombre == "--atenuacion0" ? c.atenuacion0 : c.atenuacion1) =
            static_cast<double>(millonesimas) / 1e6;
    } else if (nombre == "--canal") {
        if (valor != "izquierdo" && valor != "derecho")
            return "--canal: se esperaba izquierdo o derecho, no \"" + mostrar(valor) + "\"";
        c.canal = valor;
    } else if (nombre == "--diagnostico") {
        if (valor.empty()) return "--diagnostico: falta la ruta";
        c.diagnostico = valor;
    }
    return "";
}

}  // namespace

const std::string& uso() {
    static const std::string texto = [] {
        // Como {:g} de Python, con punto decimal aunque main cambie la locale.
        const auto g = [](double x) {
            std::ostringstream s;
            s.imbue(std::locale::classic());
            s << x;
            return s.str();
        };
        const std::string a0 = g(p::ATENUACION0);
        const std::string a1 = g(p::ATENUACION1);
        const std::string r0 = std::to_string(p::RETARDO0_US);
        const std::string r1 = std::to_string(p::RETARDO1_US);
        return std::string(
                   "Uso:\n"
                   "  ecoesteg ENTRADA.wav SALIDA.wav [metadatos] [enmascaramiento] [otras]   "
                   "codifica\n"
                   "  ecoesteg ENTRADA.wav [enmascaramiento] [otras]                          "
                   "decodifica\n"
                   "\n"
                   "Metadatos (texto UTF-8 de 1 a 255 bytes; con al menos uno, se codifica):\n"
                   "  --titulo T  --artista T  --autor T  --album T  --anio T  --invitado T  "
                   "--productor T\n"
                   "\n"
                   "Enmascaramiento (los mismos al codificar y al decodificar):\n"
                   "  --retardo0 US      retardo del eco del bit 0, en us (12 a 1000000)    [") +
               r0 +
               "]\n"
               "  --retardo1 US      retardo del eco del bit 1, en us (12 a 1000000)    [" +
               r1 +
               "]\n"
               "  --atenuacion0 P    amplitud del eco del bit 0, en % (0, 100)          [" +
               a0 +
               "]\n"
               "  --atenuacion1 P    amplitud del eco del bit 1, en % (0, 100)          [" +
               a1 +
               "]\n"
               "\n"
               "Otras:\n"
               "  --canal C          izquierdo o derecho                                "
               "[izquierdo]\n"
               "  --diagnostico F    escribe en F un CSV con el bit de cada ventana\n"
               "  --ayuda            muestra esta ayuda\n"
               "\n"
               "Códigos de salida: 0 éxito, 1 error de ejecución, 2 error de uso.\n";
    }();
    return texto;
}

const char* nombre_modo(Modo modo) {
    switch (modo) {
        case Modo::codificar: return "codificar";
        case Modo::decodificar: return "decodificar";
        case Modo::ayuda: return "ayuda";
    }
    return "";
}

std::variant<Configuracion, ErrorUso> analizar(const std::vector<std::string>& args) {
    Configuracion c;
    std::map<std::string, std::string> valores;  // opción -> último valor (R9)
    std::vector<std::string> posicionales;
    bool ayuda = false;
    bool solo_posicionales = false;
    for (std::size_t i = 0; i < args.size();) {
        const std::string& a = args[i];
        ++i;
        if (solo_posicionales || !es_opcion(a)) {
            posicionales.push_back(a);
            continue;
        }
        if (a == "--") {
            solo_posicionales = true;
            continue;
        }
        const std::size_t igual = a.find('=');
        const std::string nombre = a.substr(0, igual);
        std::string valor = igual == std::string::npos ? "" : a.substr(igual + 1);
        if (nombre == "--ayuda") {
            if (igual != std::string::npos) return ErrorUso{"--ayuda no lleva valor"};
            ayuda = true;
            continue;
        }
        if (!con_valor(nombre)) return ErrorUso{"opción desconocida: " + mostrar(nombre)};
        if (igual == std::string::npos) {
            if (i >= args.size() || es_opcion(args[i]))
                return ErrorUso{"falta el valor de " + mostrar(nombre)};
            valor = args[i];
            ++i;
        }
        valores[nombre] = valor;
    }

    for (const auto& nombre : ORDEN) {
        const auto v = valores.find(nombre);
        if (v == valores.end()) continue;
        const std::string error = asignar(c, nombre, v->second);
        if (!error.empty()) return ErrorUso{error};
    }

    if (ayuda) {
        c.modo = Modo::ayuda;
        return c;
    }
    if (!c.metadatos.empty()) {
        if (posicionales.size() != 2)
            return ErrorUso{"para codificar se necesitan ENTRADA.wav y SALIDA.wav"};
        c.modo = Modo::codificar;
        c.entrada = posicionales[0];
        c.salida = posicionales[1];
        if (c.entrada == c.salida)
            return ErrorUso{"SALIDA.wav no puede ser la misma ruta que ENTRADA.wav"};
    } else {
        if (posicionales.size() != 1)
            return ErrorUso{
                "para decodificar se necesita solo ENTRADA.wav "
                "(para codificar, al menos un metadato)"};
        c.modo = Modo::decodificar;
        c.entrada = posicionales[0];
    }

    const Enmascaramiento r = resolver(c);
    const std::int64_t m0 = p::muestras_desde_us(r.retardo0_us);
    if (m0 == p::muestras_desde_us(r.retardo1_us))
        return ErrorUso{"los retardos " + std::to_string(r.retardo0_us) + " y " +
                        std::to_string(r.retardo1_us) + " us dan las mismas " +
                        std::to_string(m0) + " muestras; deben ser distintos"};
    return c;
}

Enmascaramiento resolver(const Configuracion& c) {
    return {
        c.retardo0_us.value_or(p::RETARDO0_US),
        c.retardo1_us.value_or(p::RETARDO1_US),
        c.atenuacion0.value_or(p::ATENUACION0),
        c.atenuacion1.value_or(p::ATENUACION1),
    };
}

}  // namespace linea_comandos
