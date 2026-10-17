#include <algorithm>
#include <cmath>
#include <complex>
#include <stdexcept>
#include <string>
#include <vector>

#include "fft.hpp"
#include "pruebas.hpp"

/*
Aquí es donde propiamente se hace la comparación entre el comportamiento del
prototipo en Python y la implementación en C++
*/

namespace {

// Da la muestra de mayor magnitud en la señal. Se usa para escalar la tolerancia
// (el error de redondeo crece con el tamaño de los números)
double max_abs(const std::vector<complejo>& x) {
    double m = 0.0;
    for (const auto& v : x) m = std::max(m, std::abs(v));
    return m;
}

// Da la mayor diferencia entre dos vectores. Es una forma de darse una idea de cuan
// semejantes son dos resultados
double error_max(const std::vector<complejo>& a, const std::vector<complejo>& b) {
    double e = 0.0;
    for (std::size_t i = 0; i < a.size(); ++i) e = std::max(e, std::abs(a[i] - b[i]));
    return e;
}

}

// Comparación directa entre los resultados del prototipo y los de la implementación en c++
PRUEBA(fft_igual_que_numpy) {
    // Los tamaños van en secuencia a propósito: cada cambio de N obliga a rehacer
    // la tabla de factores de giro, así que esta prueba también la cubre.
    for (std::size_t n : {std::size_t{1}, std::size_t{2}, std::size_t{8}, std::size_t{1024}, std::size_t{4096}}) {
        const std::string base = "fft_" + std::to_string(n) + "_";
        const auto x = pruebas::leer_complejos(base + "entrada");
        const auto esperado_fft = pruebas::leer_complejos(base + "fft");
        const auto esperado_ifft = pruebas::leer_complejos(base + "ifft");
        REQUERIR(x.size() == n && esperado_fft.size() == n && esperado_ifft.size() == n);
        // El error de redondeo de la FFT crece como log2(N).
        const double tol = 1e-13 * std::log2(2.0 * static_cast<double>(n)) * max_abs(x);

        auto y = x;
        fft(y);
        VERIFICAR_CERCA(error_max(y, esperado_fft), 0.0, tol);
        y = x;
        ifft(y);  // la salida está escalada por 1/N, y su error también
        VERIFICAR_CERCA(error_max(y, esperado_ifft), 0.0, tol / static_cast<double>(n));
    }
}

// Verifica que la función se rompa donde debe hacerlo: llama a fft() con vectores de 0, 3
// 6 y 1000 elementos y verifica que devuelva std::invalid argument en cada caso. (la entrada
// debe ser una potencia de 2)
PRUEBA(fft_tamano_invalido_lanza) {
    for (std::size_t n : {std::size_t{0}, std::size_t{3}, std::size_t{6}, std::size_t{1000}}) {
        std::vector<complejo> x(n);
        bool lanzo = false;
        try {
            fft(x);
        } catch (const std::invalid_argument&) {
            lanzo = true;
        }
        VERIFICAR(lanzo);
    }
}

// Probablemente será necesario saber hasta donde rellenar con ceros cada ventana antes de la FFT,
// así que esta prueba verifica que potencia_de_2_siguiente(n) devuelva la menor potencia de 2
// que sea mayor o igual a n
PRUEBA(fft_potencia_de_2_siguiente) {
    VERIFICAR_IGUAL(potencia_de_2_siguiente(0), 1);
    VERIFICAR_IGUAL(potencia_de_2_siguiente(1), 1);
    VERIFICAR_IGUAL(potencia_de_2_siguiente(3), 4);
    VERIFICAR_IGUAL(potencia_de_2_siguiente(1024), 1024);
    VERIFICAR_IGUAL(potencia_de_2_siguiente(2112), 4096);
}
