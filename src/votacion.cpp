/*
Implementación en C++ del algoritmo en prototipo/votacion.py
*/

#include "votacion.hpp"

#include <algorithm>
#include <vector>

namespace votacion {

using trama::Bits;

namespace {

struct Puntaje {
    std::size_t periodo;
    long long coincidencias;
    long long comparaciones;
};

Bits votar(const Bits& bits, const std::vector<std::size_t>& inicios, std::size_t periodo) {
    const std::size_t n = bits.size();
    Bits votada(periodo);
    for (std::size_t i = 0; i < periodo; ++i) {
        std::size_t total = 0;
        std::size_t unos = 0;
        int primera = -1;
        for (const std::size_t s : inicios) {
            if (s + i >= n) continue;
            if (primera < 0) primera = bits[s + i];
            ++total;
            unos += static_cast<std::size_t>(bits[s + i]);
        }
        const std::size_t ceros = total - unos;
        if (total >= MIN_COPIAS && unos != ceros)
            votada[i] = unos > ceros ? 1 : 0;
        else
            votada[i] = primera;
    }
    return votada;
}

// Inicio de cada copia, buscando cada una cerca del fin de la anterior.
std::vector<std::size_t> alinear(const Bits& bits, const Bits& referencia) {
    const std::size_t n = bits.size();
    const std::size_t periodo = referencia.size();
    std::vector<long long> desfases = {0};
    for (int d = 1; d <= DERIVA_MAX; ++d) {
        desfases.push_back(-d);
        desfases.push_back(d);
    }
    std::vector<std::size_t> inicios = {0};
    while (inicios.back() + periodo < n) {
        const auto esperado = static_cast<long long>(inicios.back() + periodo);
        std::size_t mejor = static_cast<std::size_t>(esperado);
        long long c_mejor = -1;
        long long t_mejor = 1;
        for (const long long d : desfases) {
            const long long s = esperado + d;
            if (s <= static_cast<long long>(inicios.back()) || s >= static_cast<long long>(n))
                continue;
            const auto inicio = static_cast<std::size_t>(s);
            const std::size_t t = std::min(periodo, n - inicio);
            long long c = 0;
            for (std::size_t i = 0; i < t; ++i) c += bits[inicio + i] == referencia[i];
            if (c * t_mejor > c_mejor * static_cast<long long>(t)) {  // empate: el anterior
                mejor = inicio;
                c_mejor = c;
                t_mejor = static_cast<long long>(t);
            }
        }
        inicios.push_back(mejor);
    }
    return inicios;
}

}  // namespace

std::optional<std::size_t> estimar_periodo(const Bits& b) {
    const std::size_t n = b.size();
    std::vector<Puntaje> puntajes;
    const std::size_t hasta = std::min(n / MIN_COPIAS, PERIODO_MAX);
    for (std::size_t periodo = PERIODO_MIN; periodo <= hasta; ++periodo) {
        const std::size_t bloques = (n - periodo - DERIVA_MAX) / BLOQUE;
        if (bloques == 0) break;
        long long total = 0;
        for (std::size_t j = 0; j < bloques; ++j) {
            long long mejor = 0;
            for (int d = -DERIVA_MAX; d <= DERIVA_MAX; ++d) {
                const std::size_t desde = periodo + static_cast<std::size_t>(d + DERIVA_MAX) -
                                          static_cast<std::size_t>(DERIVA_MAX);
                long long c = 0;
                for (std::size_t i = j * BLOQUE; i < (j + 1) * BLOQUE; ++i)
                    c += b[i] == b[desde + i];
                mejor = std::max(mejor, c);
            }
            total += mejor;
        }
        puntajes.push_back({periodo, total, static_cast<long long>(bloques * BLOQUE)});
    }
    if (puntajes.empty()) return std::nullopt;

    long long c_max = puntajes[0].coincidencias;
    long long t_max = puntajes[0].comparaciones;
    for (const auto& q : puntajes)
        if (q.coincidencias * t_max > c_max * q.comparaciones) {
            c_max = q.coincidencias;
            t_max = q.comparaciones;
        }
    if (1000 * c_max < UMBRAL_MILESIMAS * t_max) return std::nullopt;

    std::size_t candidato = puntajes[0].periodo;
    for (const auto& q : puntajes)  // c/t >= c_max/t_max - margen
        if (1000 * q.coincidencias * t_max >=
            1000 * c_max * q.comparaciones - MARGEN_MILESIMAS * q.comparaciones * t_max) {
            candidato = q.periodo;
            break;
        }

    // La tolerancia hace que P - 4 puntúe igual que P: gana el que más coincide
    // sin corrimiento.
    std::size_t mejor = candidato;
    long long c_mejor = -1;
    long long t_mejor = 1;
    const std::size_t fin = std::min(candidato + 2 * DERIVA_MAX, n / MIN_COPIAS);
    for (std::size_t periodo = candidato; periodo <= fin; ++periodo) {
        long long c = 0;
        for (std::size_t i = 0; i + periodo < n; ++i) c += b[i] == b[i + periodo];
        const auto t = static_cast<long long>(n - periodo);
        if (c * t_mejor > c_mejor * t) {  // en un empate, el menor
            mejor = periodo;
            c_mejor = c;
            t_mejor = t;
        }
    }
    return mejor;
}

std::optional<Bits> votar_copias(const Bits& bits) {
    const auto periodo = estimar_periodo(bits);
    if (!periodo) return std::nullopt;
    std::vector<std::size_t> ingenuos;
    for (std::size_t s = 0; s < bits.size(); s += *periodo) ingenuos.push_back(s);
    Bits votada = votar(bits, ingenuos, *periodo);
    for (int k = 0; k < PASADAS; ++k) votada = votar(bits, alinear(bits, votada), *periodo);
    return votada;
}

}  // namespace votacion
