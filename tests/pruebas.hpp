// Infraestructura de pruebas de C++ del proyecto, sin dependencias externas.
//
// Cada dueño escribe sus pruebas en tests/pruebas_<módulo>.cpp:
//
//     #include "pruebas.hpp"
//     #include "parametros.hpp"
//
//     PRUEBA(muestras_desde_us_redondea) {
//         VERIFICAR_IGUAL(muestras_desde_us(1000), 44);
//     }
//
// El Makefile enlaza todos los tests/*.cpp en bin/pruebas, y tests/principal.cpp
// tiene el único main. Cada PRUEBA se anota sola en el registro al arrancar el
// programa, así que ningún archivo necesita conocer a los demás. Los nombres de
// las pruebas deben ser únicos en todo el proyecto: principal.cpp falla si hay
// dos iguales.
//
// Verificaciones (cada fallo se reporta con archivo, línea y expresión):
//   VERIFICAR(cond)                      la condición es verdadera.
//   VERIFICAR_IGUAL(obtenido, esperado)  igualdad exacta; compara bien enteros
//                                        con y sin signo, cadenas de C y vectores.
//   VERIFICAR_CERCA(obtenido, esperado, tol)
//                                        |obtenido − esperado| ≤ tol; para reales
//                                        y complejos.
//   REQUERIR(cond)                       como VERIFICAR, pero si falla termina la
//                                        prueba (por ejemplo, antes de indexar un
//                                        vector cuyo tamaño no se cumplió).
// Las tres primeras siguen después de un fallo, para ver todos de una vez.
//
// Vectores de referencia de prototipo/vectores.py (tests/vectores/<nombre>.txt):
//   leer_reales(nombre), leer_complejos(nombre), leer_enteros(nombre),
//   leer_textos(nombre)        valores ya convertidos; lanzan si el tipo no es
//                              el de la cabecera o el archivo está mal formado.
//   leer_lineas(nombre), desescapar(linea)
//                              para formatos propios (las líneas sin convertir).

#ifndef PRUEBAS_HPP
#define PRUEBAS_HPP

#include <cctype>
#include <cerrno>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace pruebas {

struct Prueba {
    const char* nombre;
    void (*funcion)();
};

// El registro vive dentro de una función: C++ no define en qué orden se
// inicializan las variables globales de archivos distintos, y una prueba podría
// anotarse antes de que existiera una lista global. Una variable estática local
// se crea la primera vez que se usa.
inline std::vector<Prueba>& registro() {
    static std::vector<Prueba> r;
    return r;
}

// Mensajes de fallo de la prueba que se está corriendo; principal.cpp los
// vacía antes de cada prueba y los imprime al terminarla.
inline std::vector<std::string>& fallos() {
    static std::vector<std::string> f;
    return f;
}

struct Registrador {
    Registrador(const char* nombre, void (*funcion)()) {
        registro().push_back({nombre, funcion});
    }
};

inline void reportar_fallo(const char* archivo, int linea, const std::string& mensaje) {
    fallos().push_back(std::string(archivo) + ":" + std::to_string(linea) + ": " + mensaje);
}

// ---- Detalles de implementación ------------------------------------------

template <typename T>
using sin_cv_ref = std::remove_cv_t<std::remove_reference_t<T>>;

template <typename T>
constexpr bool es_cadena_c = std::is_same_v<std::decay_t<T>, const char*> ||
                             std::is_same_v<std::decay_t<T>, char*>;

template <typename T>
constexpr bool es_entero = std::is_integral_v<T> && !std::is_same_v<T, bool>;

template <typename T>
struct es_vector : std::false_type {};
template <typename T, typename A>
struct es_vector<std::vector<T, A>> : std::true_type {};

template <typename T, typename = void>
struct es_imprimible : std::false_type {};
template <typename T>
struct es_imprimible<T, std::void_t<decltype(std::declval<std::ostream&>()
                                             << std::declval<const T&>())>>
    : std::true_type {};

// Igualdad sin -Wsign-compare (que con -Werror rompe la CI): entre enteros de
// distinto signo, primero se revisa que el con signo no sea negativo y después
// se comparan como unsigned long long. C++17 no tiene std::cmp_equal.
template <typename A, typename B>
bool iguales(const A& a, const B& b) {
    if constexpr (es_cadena_c<A> || es_cadena_c<B>) {
        return std::string_view(a) == std::string_view(b);
    } else if constexpr (es_entero<A> && es_entero<B> &&
                         std::is_signed_v<A> != std::is_signed_v<B>) {
        if constexpr (std::is_signed_v<A>) {
            return a >= 0 && static_cast<unsigned long long>(a) ==
                                 static_cast<unsigned long long>(b);
        } else {
            return b >= 0 && static_cast<unsigned long long>(a) ==
                                 static_cast<unsigned long long>(b);
        }
    } else if constexpr (es_vector<A>::value && es_vector<B>::value) {
        if (a.size() != b.size()) return false;
        for (std::size_t i = 0; i < a.size(); ++i)
            if (!iguales(a[i], b[i])) return false;
        return true;
    } else {
        return a == b;
    }
}

template <typename T>
std::string mostrar(const T& v) {
    std::ostringstream s;
    if constexpr (std::is_same_v<T, bool>) {
        s << (v ? "true" : "false");
    } else if constexpr (std::is_same_v<T, char> || std::is_same_v<T, signed char> ||
                         std::is_same_v<T, unsigned char>) {
        s << static_cast<int>(v);  // uint8_t se imprime como número, no como carácter
    } else if constexpr (std::is_floating_point_v<T>) {
        s.precision(17);
        s << v;
    } else if constexpr (es_cadena_c<T> || std::is_same_v<T, std::string> ||
                         std::is_same_v<T, std::string_view>) {
        s << '"' << v << '"';
    } else if constexpr (es_imprimible<T>::value) {
        s.precision(17);
        s << v;
    } else {
        s << "<no imprimible>";
    }
    return s.str();
}

// Para vectores dice el tamaño o el primer índice distinto en vez de imprimirlos
// enteros (una trama tiene cientos de bits).
template <typename A, typename B>
std::string diferencia(const A& a, const B& b) {
    if constexpr (es_vector<A>::value && es_vector<B>::value) {
        if (a.size() != b.size())
            return "tamaño " + std::to_string(a.size()) + ", se esperaba " +
                   std::to_string(b.size());
        for (std::size_t i = 0; i < a.size(); ++i)
            if (!iguales(a[i], b[i]))
                return "en [" + std::to_string(i) + "] se obtuvo " + mostrar(a[i]) +
                       ", se esperaba " + mostrar(b[i]);
        return "iguales";  // no se llega: solo se llama si difieren
    } else {
        return "se obtuvo " + mostrar(a) + ", se esperaba " + mostrar(b);
    }
}

template <typename A, typename B, typename T>
bool cerca(const A& a, const B& b, const T& tol, std::string& detalle) {
    double dif;
    if constexpr (std::is_arithmetic_v<A> && std::is_arithmetic_v<B>)
        dif = std::fabs(static_cast<double>(a) - static_cast<double>(b));
    else
        dif = static_cast<double>(std::abs(a - b));  // complejos
    std::ostringstream s;
    s.precision(17);
    s << "se obtuvo " << a << ", se esperaba " << b << ", diferencia " << dif
      << " > " << tol;
    detalle = s.str();
    return dif <= static_cast<double>(tol);  // NaN da falso: falla
}

// ---- Vectores de referencia -------------------------------------------------
//
// Archivos tests/vectores/<nombre>.txt que escribe prototipo/vectores.py (el
// formato está en su docstring). Las rutas son relativas a la raíz del
// repositorio, desde donde corre make test. Un error de lectura lanza una
// excepción, que principal.cpp reporta como fallo de la prueba.

inline std::runtime_error error_vector(const std::string& nombre, std::size_t linea,
                                       const std::string& mensaje) {
    return std::runtime_error("tests/vectores/" + nombre + ".txt:" +
                              std::to_string(linea) + ": " + mensaje);
}

// Líneas del vector sin la cabecera, tal como están en el archivo (sin
// desescapar). Si tipo no es vacío, verifica que la cabecera diga tipo=<tipo>.
inline std::vector<std::string> leer_lineas(const std::string& nombre,
                                            const std::string& tipo = "") {
    std::ifstream f("tests/vectores/" + nombre + ".txt", std::ios::binary);
    if (!f)
        throw error_vector(nombre, 0,
                           "no se pudo abrir (¿se corrió desde la raíz del repositorio? "
                           "¿falta make vectores?)");
    std::string cabecera;
    std::getline(f, cabecera);
    if (cabecera.rfind("# ", 0) != 0) throw error_vector(nombre, 1, "falta la cabecera");
    if (!tipo.empty()) {
        const std::string sufijo = ", tipo=" + tipo;
        if (cabecera.size() < sufijo.size() ||
            cabecera.compare(cabecera.size() - sufijo.size(), sufijo.size(), sufijo) != 0)
            throw error_vector(nombre, 1, "se esperaba tipo=" + tipo);
    }
    std::vector<std::string> lineas;
    for (std::string l; std::getline(f, l);) lineas.push_back(l);
    return lineas;
}

// Inversa del escape de prototipo/vectores.py: \\, \n y \xNN.
inline std::string desescapar(const std::string& linea) {
    std::string salida;
    for (std::size_t i = 0; i < linea.size(); ++i) {
        if (linea[i] != '\\') {
            salida += linea[i];
        } else if (i + 1 < linea.size() && linea[i + 1] == '\\') {
            salida += '\\';
            i += 1;
        } else if (i + 1 < linea.size() && linea[i + 1] == 'n') {
            salida += '\n';
            i += 1;
        } else if (i + 3 < linea.size() && linea[i + 1] == 'x' &&
                   std::isxdigit(static_cast<unsigned char>(linea[i + 2])) &&
                   std::isxdigit(static_cast<unsigned char>(linea[i + 3]))) {
            salida += static_cast<char>(std::stoi(linea.substr(i + 2, 2), nullptr, 16));
            i += 3;
        } else {
            throw std::runtime_error("escape inválido en «" + linea + "»");
        }
    }
    return salida;
}

// Convierte un texto completo a double; falla si sobra algo.
inline double a_real(const std::string& nombre, std::size_t linea, const std::string& s) {
    char* fin = nullptr;  // sin revisar ERANGE: strtod lo marca también en subnormales
    const double x = std::strtod(s.c_str(), &fin);
    if (s.empty() || *fin != '\0')
        throw error_vector(nombre, linea, "no es un real: «" + s + "»");
    return x;
}

inline std::vector<double> leer_reales(const std::string& nombre) {
    const auto lineas = leer_lineas(nombre, "real");
    std::vector<double> v;
    for (std::size_t i = 0; i < lineas.size(); ++i)
        v.push_back(a_real(nombre, i + 2, lineas[i]));
    return v;
}

inline std::vector<std::complex<double>> leer_complejos(const std::string& nombre) {
    const auto lineas = leer_lineas(nombre, "complejo");
    std::vector<std::complex<double>> v;
    for (std::size_t i = 0; i < lineas.size(); ++i) {
        const auto espacio = lineas[i].find(' ');
        if (espacio == std::string::npos)
            throw error_vector(nombre, i + 2, "se esperaba «re im»");
        v.emplace_back(a_real(nombre, i + 2, lineas[i].substr(0, espacio)),
                       a_real(nombre, i + 2, lineas[i].substr(espacio + 1)));
    }
    return v;
}

inline std::vector<long long> leer_enteros(const std::string& nombre) {
    const auto lineas = leer_lineas(nombre, "entero");
    std::vector<long long> v;
    for (std::size_t i = 0; i < lineas.size(); ++i) {
        errno = 0;
        char* fin = nullptr;
        const long long x = std::strtoll(lineas[i].c_str(), &fin, 10);
        if (lineas[i].empty() || *fin != '\0' || errno == ERANGE)
            throw error_vector(nombre, i + 2, "no es un entero: «" + lineas[i] + "»");
        v.push_back(x);
    }
    return v;
}

// Cadenas como bytes (UTF-8 tal cual, con los bytes inválidos que hubiera).
inline std::vector<std::string> leer_textos(const std::string& nombre) {
    const auto lineas = leer_lineas(nombre, "texto");
    std::vector<std::string> v;
    for (std::size_t i = 0; i < lineas.size(); ++i) {
        try {
            v.push_back(desescapar(lineas[i]));
        } catch (const std::runtime_error& e) {
            throw error_vector(nombre, i + 2, e.what());
        }
    }
    return v;
}

}  // namespace pruebas

// ---- Macros ---------------------------------------------------------------

#define PRUEBA(nombre)                                                        \
    static void prueba_##nombre();                                            \
    static const ::pruebas::Registrador registrador_##nombre(#nombre,         \
                                                             &prueba_##nombre); \
    static void prueba_##nombre()

#define VERIFICAR(cond)                                                       \
    do {                                                                      \
        if (!(cond))                                                          \
            ::pruebas::reportar_fallo(__FILE__, __LINE__, "VERIFICAR(" #cond ")"); \
    } while (0)

#define REQUERIR(cond)                                                        \
    do {                                                                      \
        if (!(cond)) {                                                        \
            ::pruebas::reportar_fallo(__FILE__, __LINE__, "REQUERIR(" #cond ")"); \
            return;                                                           \
        }                                                                     \
    } while (0)

#define VERIFICAR_IGUAL(obtenido, esperado)                                   \
    do {                                                                      \
        const auto& pruebas_o_ = (obtenido);                                  \
        const auto& pruebas_e_ = (esperado);                                  \
        if (!::pruebas::iguales(pruebas_o_, pruebas_e_))                      \
            ::pruebas::reportar_fallo(                                        \
                __FILE__, __LINE__,                                           \
                "VERIFICAR_IGUAL(" #obtenido ", " #esperado "): " +           \
                    ::pruebas::diferencia(pruebas_o_, pruebas_e_));           \
    } while (0)

#define VERIFICAR_CERCA(obtenido, esperado, tol)                              \
    do {                                                                      \
        std::string pruebas_d_;                                               \
        if (!::pruebas::cerca((obtenido), (esperado), (tol), pruebas_d_))     \
            ::pruebas::reportar_fallo(                                        \
                __FILE__, __LINE__,                                           \
                "VERIFICAR_CERCA(" #obtenido ", " #esperado ", " #tol "): " + \
                    pruebas_d_);                                              \
    } while (0)

#endif  // PRUEBAS_HPP
