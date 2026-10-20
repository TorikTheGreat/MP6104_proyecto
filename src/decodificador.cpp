// Dueño: B.
#include "decodificador.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

#include "fft.hpp"

namespace {

std::vector<double> a_reales(const std::vector<std::int16_t>& x) {
    std::vector<double> salida(x.size());
    for (std::size_t i = 0; i < x.size(); ++i) {
        salida[i] = static_cast<double>(x[i]);
    }
    return salida;
}

}  // namespace

double rms_ventana(const std::vector<double>& ventana) {
    if (ventana.empty()) {
        throw std::invalid_argument("rms_ventana: la ventana no puede estar vacía");
    }
    double suma = 0.0;
    for (double v : ventana) {
        if (!std::isfinite(v)) {
            throw std::invalid_argument("rms_ventana: muestra NaN o infinita");
        }
        suma += v * v;
    }
    return std::sqrt(suma / static_cast<double>(ventana.size()));
}

double rms_ventana(const std::vector<std::int16_t>& ventana) {
    return rms_ventana(a_reales(ventana));
}

std::vector<double> normalizar_ventana(const std::vector<double>& ventana) {
    const double rms = rms_ventana(ventana);
    std::vector<double> salida(ventana.size(), 0.0);
    if (rms <= UMBRAL_SILENCIO) {
        return salida;
    }
    const double factor = RMS_REFERENCIA / rms;
    for (std::size_t i = 0; i < ventana.size(); ++i) {
        salida[i] = ventana[i] * factor;
    }
    return salida;
}

std::vector<double> normalizar_ventana(const std::vector<std::int16_t>& ventana) {
    return normalizar_ventana(a_reales(ventana));
}

std::vector<double> autocorrelacion_cepstro(const std::vector<double>& ventana) {
    const auto v_norm = normalizar_ventana(ventana);
    const std::size_t n_fft = potencia_de_2_siguiente(v_norm.size());
    bool silenciosa = true;
    for (double v : v_norm) {
        if (v != 0.0) {
            silenciosa = false;
            break;
        }
    }
    if (silenciosa) {
        return std::vector<double>(n_fft, 0.0);
    }

    std::vector<complejo> espectro(n_fft, complejo(0.0, 0.0));
    for (std::size_t i = 0; i < v_norm.size(); ++i) {
        espectro[i] = complejo(v_norm[i], 0.0);
    }
    fft(espectro);

    for (std::size_t k = 0; k < n_fft; ++k) {
        const double mag = std::max(std::abs(espectro[k]), EPSILON_ESPECTRO);
        const double lg = std::log(mag);
        espectro[k] = complejo(lg * lg, 0.0);
    }
    ifft(espectro);

    std::vector<double> rcc(n_fft);
    for (std::size_t n = 0; n < n_fft; ++n) {
        rcc[n] = std::abs(espectro[n]);
    }
    return rcc;
}

std::vector<double> autocorrelacion_cepstro(const std::vector<std::int16_t>& ventana) {
    return autocorrelacion_cepstro(a_reales(ventana));
}

DiagnosticoVentana evaluar_ventana(const std::vector<double>& ventana, std::size_t d0,
                                   std::size_t d1, double umbral_eco, std::size_t indice) {
    if (ventana.empty()) {
        throw std::invalid_argument("evaluar_ventana: la ventana no puede estar vacía");
    }
    if (d0 == 0 || d1 == 0 || d0 == d1) {
        throw std::invalid_argument("evaluar_ventana: retardos inválidos");
    }
    if (std::max(d0, d1) >= ventana.size()) {
        throw std::invalid_argument(
            "evaluar_ventana: los retardos deben ser menores que el largo de la ventana");
    }
    if (!std::isfinite(umbral_eco) || umbral_eco < 0.0) {
        throw std::invalid_argument("evaluar_ventana: umbral_eco inválido");
    }

    const double rms = rms_ventana(ventana);
    if (rms <= UMBRAL_SILENCIO) {
        return DiagnosticoVentana{indice, SIN_ECO, 0.0, 0.0, rms};
    }

    const auto rcc = autocorrelacion_cepstro(ventana);
    const double r0 = rcc[d0];
    const double r1 = rcc[d1];
    const double pico = std::max(r0, r1);
    int bit = SIN_ECO;
    if (pico >= umbral_eco && r0 != r1) {
        bit = (r0 > r1) ? 0 : 1;
    }
    return DiagnosticoVentana{indice, bit, r0, r1, rms};
}

DiagnosticoVentana evaluar_ventana(const std::vector<std::int16_t>& ventana, std::size_t d0,
                                   std::size_t d1, double umbral_eco, std::size_t indice) {
    return evaluar_ventana(a_reales(ventana), d0, d1, umbral_eco, indice);
}

int clasificar_ventana(const std::vector<double>& ventana, std::size_t d0, std::size_t d1,
                       double umbral_eco) {
    return evaluar_ventana(ventana, d0, d1, umbral_eco).bit;
}

int clasificar_ventana(const std::vector<std::int16_t>& ventana, std::size_t d0, std::size_t d1,
                       double umbral_eco) {
    return evaluar_ventana(a_reales(ventana), d0, d1, umbral_eco).bit;
}

ResultadoDecodificacion decodificar_senal(const std::vector<double>& senal, std::size_t d0,
                                          std::size_t d1, std::size_t largo_ventana,
                                          double umbral_eco) {
    if (d0 == 0 || d1 == 0 || d0 == d1) {
        throw std::invalid_argument("decodificar_senal: retardos inválidos");
    }
    const std::size_t max_d = std::max(d0, d1);
    const std::size_t largo =
        (largo_ventana == 0)
            ? static_cast<std::size_t>(parametros::K_VENTANA) * max_d
            : largo_ventana;
    if (largo <= max_d) {
        throw std::invalid_argument(
            "decodificar_senal: el largo de ventana debe ser mayor que los retardos");
    }

    const std::size_t n_ventanas = senal.size() / largo;
    std::vector<DiagnosticoVentana> ventanas;
    ventanas.reserve(n_ventanas);
    std::size_t no_silenciosas = 0;
    std::size_t con_eco = 0;

    for (std::size_t k = 0; k < n_ventanas; ++k) {
        const auto ini = senal.begin() + static_cast<std::ptrdiff_t>(k * largo);
        std::vector<double> sub(ini, ini + static_cast<std::ptrdiff_t>(largo));
        auto diag = evaluar_ventana(sub, d0, d1, umbral_eco, k);
        if (diag.rms > UMBRAL_SILENCIO) {
            ++no_silenciosas;
            if (diag.bit != SIN_ECO) {
                ++con_eco;
            }
        }
        ventanas.push_back(diag);
    }

    if (no_silenciosas == 0 ||
        static_cast<double>(con_eco) <
            FRACCION_MIN_VENTANAS_CON_ECO * static_cast<double>(no_silenciosas)) {
        return ResultadoDecodificacion{{}, ventanas};
    }

    std::size_t fin_activo = ventanas.size();
    std::size_t sin_eco_finales = 0;
    for (std::size_t k = ventanas.size(); k > 0; --k) {
        if (ventanas[k - 1].bit == SIN_ECO) {
            ++sin_eco_finales;
        } else {
            break;
        }
    }
    if (sin_eco_finales >= 4) {
        fin_activo = ventanas.size() - sin_eco_finales;
    }

    std::vector<int> bits;
    bits.reserve(fin_activo);
    for (std::size_t k = 0; k < fin_activo; ++k) {
        if (ventanas[k].rms <= UMBRAL_SILENCIO) {
            continue;
        }
        bits.push_back((ventanas[k].r0 >= ventanas[k].r1) ? 0 : 1);
    }

    return ResultadoDecodificacion{bits, ventanas};
}

ResultadoDecodificacion decodificar_senal(const std::vector<std::int16_t>& senal, std::size_t d0,
                                          std::size_t d1, std::size_t largo_ventana,
                                          double umbral_eco) {
    return decodificar_senal(a_reales(senal), d0, d1, largo_ventana, umbral_eco);
}

ResultadoDecodificacion decodificar_por_us(const std::vector<double>& senal,
                                           std::int64_t retardo0_us, std::int64_t retardo1_us,
                                           double umbral_eco) {
    const auto d0 = static_cast<std::size_t>(parametros::muestras_desde_us(retardo0_us));
    const auto d1 = static_cast<std::size_t>(parametros::muestras_desde_us(retardo1_us));
    const auto largo =
        static_cast<std::size_t>(parametros::largo_ventana(retardo0_us, retardo1_us));
    return decodificar_senal(senal, d0, d1, largo, umbral_eco);
}

ResultadoDecodificacion decodificar_por_us(const std::vector<std::int16_t>& senal,
                                           std::int64_t retardo0_us, std::int64_t retardo1_us,
                                           double umbral_eco) {
    return decodificar_por_us(a_reales(senal), retardo0_us, retardo1_us, umbral_eco);
}
