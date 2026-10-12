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

#ifndef PRUEBAS_HPP
#define PRUEBAS_HPP

#include <cmath>
#include <complex>
#include <cstddef>
#include <sstream>
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
