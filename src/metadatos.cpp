#include "metadatos.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <utility>

#include "trama_registros.hpp"
#include "votacion.hpp"

namespace metadatos {

using trama::Bits;
using trama::Lectura;
using trama::Tipo;

// En Python el formato se elige con FORMATO_TRAMA; aquí hay uno solo.
static_assert(parametros::FORMATO_TRAMA == parametros::FormatoTrama::registros,
              "metadatos.cpp solo sabe usar el formato registros");
namespace formato = trama_registros;

NoCabe::NoCabe(std::size_t necesarios, std::size_t disponibles)
    : std::runtime_error("los metadatos necesitan " + std::to_string(necesarios) +
                         " bits y caben " + std::to_string(disponibles)),
      necesarios(necesarios),
      disponibles(disponibles) {}

std::size_t bits_necesarios(const trama::Metadatos& metadatos) {
    return formato::codificar(metadatos).size();
}

Bits repetir(const Bits& bits, std::size_t n) {
    if (n > 0 && bits.empty()) throw std::invalid_argument("no hay bits que repetir");
    Bits r(n);
    for (std::size_t i = 0; i < n; ++i) r[i] = bits[i % bits.size()];
    return r;
}

Codificacion a_bits(const trama::Metadatos& metadatos, std::size_t n_ventanas,
                    parametros::SiNoCabe si_no_cabe, bool repetir_mensaje) {
    Bits copia = formato::codificar(metadatos);
    const std::size_t necesarios = copia.size();
    if (necesarios > n_ventanas) {
        if (si_no_cabe == parametros::SiNoCabe::error) throw NoCabe(necesarios, n_ventanas);
        copia.resize(n_ventanas);
        return {copia, necesarios, true};
    }
    if (repetir_mensaje) return {repetir(copia, n_ventanas), necesarios, false};
    return {copia, necesarios, false};
}

std::vector<Lectura> leer_votada(const Bits& bits) {
    const auto votada = votacion::votar_copias(bits);
    if (!votada) return {};
    Bits dos_veces = *votada;
    dos_veces.insert(dos_veces.end(), votada->begin(), votada->end());
    std::vector<Lectura> lecturas;
    for (const auto& l : formato::leer(dos_veces))
        if (l.tipo == Tipo::valido && std::find(lecturas.begin(), lecturas.end(), l) == lecturas.end())
            lecturas.push_back(l);
    return lecturas;
}

std::vector<Lectura> desde_bits(const Bits& bits, bool avisos_requieren_valido,
                                bool votar_copias) {
    std::vector<Lectura> lecturas = formato::leer(bits);
    if (votar_copias) {
        const auto votadas = leer_votada(bits);
        lecturas.insert(lecturas.end(), votadas.begin(), votadas.end());
    }

    // Textos válidos por categoría con sus copias, en orden de aparición.
    std::map<int, std::vector<std::pair<std::string, int>>> copias_validas;
    for (const auto& l : lecturas) {
        if (l.tipo != Tipo::valido) continue;
        auto& textos = copias_validas[l.categoria];
        bool visto = false;
        for (auto& [texto, n] : textos)
            if (texto == l.datos) {
                ++n;
                visto = true;
            }
        if (!visto) textos.emplace_back(l.datos, 1);
    }
    const bool avisar = !copias_validas.empty() || !avisos_requieren_valido;

    std::vector<Lectura> resultados;
    std::set<std::tuple<Tipo, int, std::string>> vistos;
    for (const auto& l : lecturas) {
        std::tuple<Tipo, int, std::string> clave;
        std::string datos = l.datos;
        if (l.tipo == Tipo::valido) {
            clave = {Tipo::valido, l.categoria, ""};
            int mas_copias = 0;
            for (const auto& [texto, n] : copias_validas[l.categoria])
                if (n > mas_copias) {  // estricto: en un empate gana el primero
                    mas_copias = n;
                    datos = texto;
                }
        } else if (l.tipo == Tipo::danado) {
            if (!avisar || copias_validas.count(l.categoria) > 0) continue;
            clave = {Tipo::danado, 0, l.datos};  // sin categoría, como en Python
        } else {
            if (!avisar) continue;
            clave = {Tipo::desconocido, l.categoria, l.datos};
        }
        if (vistos.insert(clave).second) resultados.push_back({l.tipo, l.categoria, datos});
    }
    return resultados;
}

}  // namespace metadatos
