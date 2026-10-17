#include "fft.hpp"

#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

// w[k] = exp(-j*2*pi*k/N), k = 0 .. N/2 - 1, del último N pedido.
const std::vector<complejo>& factores_de_giro(std::size_t n) {
    static std::vector<complejo> w;
    static std::size_t n_tabla = 0;
    if (n != n_tabla) {
        const double pi = std::acos(-1.0);
        w.resize(n / 2);
        for (std::size_t k = 0; k < n / 2; ++k)
            w[k] = std::polar(1.0, -2.0 * pi * static_cast<double>(k) / static_cast<double>(n));
        n_tabla = n;
    }
    return w;
}

void transformar(std::vector<complejo>& x, bool inversa) {
    const std::size_t n = x.size();
    if (n == 0 || (n & (n - 1)) != 0)
        throw std::invalid_argument("fft: el tamaño debe ser potencia de 2, no " +
                                    std::to_string(n));

    // Permutación por inversión de bits.
    for (std::size_t i = 1, j = 0; i < n; ++i) {
        std::size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(x[i], x[j]);
    }

    // Etapas de mariposas: la de tamaño m usa w_m^k = w_N^(k*N/m).
    const auto& w = factores_de_giro(n);
    for (std::size_t m = 2; m <= n; m <<= 1) {
        const std::size_t mitad = m / 2;
        const std::size_t paso = n / m;
        for (std::size_t inicio = 0; inicio < n; inicio += m) {
            for (std::size_t k = 0; k < mitad; ++k) {
                const complejo giro = inversa ? std::conj(w[k * paso]) : w[k * paso];
                const complejo t = giro * x[inicio + k + mitad];
                const complejo u = x[inicio + k];
                x[inicio + k] = u + t;
                x[inicio + k + mitad] = u - t;
            }
        }
    }

    if (inversa) {
        const double escala = 1.0 / static_cast<double>(n);
        for (auto& v : x) v *= escala;
    }
}

}  // namespace

void fft(std::vector<complejo>& x) { transformar(x, false); }

void ifft(std::vector<complejo>& x) { transformar(x, true); }

std::size_t potencia_de_2_siguiente(std::size_t n) {
    constexpr std::size_t mayor = (std::numeric_limits<std::size_t>::max() >> 1) + 1;
    if (n > mayor) throw std::overflow_error("potencia_de_2_siguiente: no cabe en size_t");
    std::size_t p = 1;
    while (p < n) p <<= 1;
    return p;
}
