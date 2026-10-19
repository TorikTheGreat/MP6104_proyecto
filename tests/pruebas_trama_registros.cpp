// Pruebas de src/trama_registros.* (las mismas que prototipo/tests/test_trama_registros.py)
// y comparación con los vectores de prototipo/vectores_metadatos.py.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "parametros.hpp"
#include "pruebas.hpp"
#include "trama_de_prueba.hpp"
#include "trama_registros.hpp"
#include "utf8.hpp"

using namespace parametros;
using trama::Bits;
using trama::Lectura;
using trama::Tipo;
namespace f = trama_registros;

namespace {

Bits repetir(const Bits& bits, int veces) {
    Bits todo;
    for (int k = 0; k < veces; ++k) todo.insert(todo.end(), bits.begin(), bits.end());
    return todo;
}

Bits desde(const Bits& bits, std::size_t corte) {
    return Bits(bits.begin() + static_cast<std::ptrdiff_t>(corte), bits.end());
}

std::vector<std::pair<Tipo, int>> tipos(const std::vector<Lectura>& lecturas) {
    std::vector<std::pair<Tipo, int>> t;
    for (const auto& l : lecturas) t.emplace_back(l.tipo, l.categoria);
    return t;
}

long contar(const std::vector<std::pair<Tipo, int>>& t, Tipo tipo, int categoria) {
    return std::count(t.begin(), t.end(), std::make_pair(tipo, categoria));
}

trama::Metadatos validos(const std::vector<Lectura>& lecturas) {
    trama::Metadatos m;
    for (const auto& l : lecturas)
        if (l.tipo == Tipo::valido) m[l.categoria] = l.datos;
    return m;
}

bool lanza_invalid_argument(const trama::Metadatos& m) {
    try {
        f::codificar(m);
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

}  // namespace

// ---- Comparación con Python -------------------------------------------------

PRUEBA(trama_crc_igual_que_python) {
    const auto entradas = pruebas::leer_textos("metadatos_crc_entradas");
    const auto salidas = pruebas::leer_enteros("metadatos_crc_salidas");
    REQUERIR(!entradas.empty() && entradas.size() == salidas.size());
    for (std::size_t i = 0; i < entradas.size(); ++i)
        VERIFICAR_IGUAL(f::crc16(entradas[i]), salidas[i]);
}

PRUEBA(trama_codificar_igual_que_python) {
    VERIFICAR_IGUAL(f::codificar(SIETE), pruebas::leer_enteros("metadatos_siete_bits"));
}

// ---- Codificar --------------------------------------------------------------

PRUEBA(trama_crc_valor_de_prueba) { VERIFICAR_IGUAL(f::crc16("123456789"), 0x29B1); }

PRUEBA(trama_un_registro) {
    const Bits bits = f::codificar({{TITULO, "A"}});
    VERIFICAR_IGUAL(bits.size(), 48);
    VERIFICAR_IGUAL(bits, registro(TITULO, "A"));
}

PRUEBA(trama_largo_de_una_copia) {
    VERIFICAR_IGUAL(f::codificar({{TITULO, "Hola"}}).size(), 72);
    std::size_t esperado = 0;
    for (const auto& [categoria, texto] : SIETE) esperado += 40 + 8 * texto.size();
    VERIFICAR_IGUAL(f::codificar(SIETE).size(), esperado);
}

PRUEBA(trama_orden_por_categoria) {
    VERIFICAR_IGUAL(f::codificar({{ARTISTA, "b"}, {TITULO, "a"}}),
                    unir({f::codificar({{TITULO, "a"}}), f::codificar({{ARTISTA, "b"}})}));
}

PRUEBA(trama_rechaza_vacio_largo_y_categoria) {
    VERIFICAR(lanza_invalid_argument({}));
    VERIFICAR(lanza_invalid_argument({{TITULO, ""}}));
    VERIFICAR(lanza_invalid_argument({{TITULO, std::string(256, 'x')}}));
    VERIFICAR(lanza_invalid_argument({{8, "x"}}));
    VERIFICAR(!lanza_invalid_argument({{TITULO, std::string(255, 'x')}}));
}

// ---- Leer -------------------------------------------------------------------

PRUEBA(trama_ida_y_vuelta) { VERIFICAR(validos(f::leer(f::codificar(SIETE))) == SIETE); }

PRUEBA(trama_desde_cualquier_desfase) {
    const Bits bits = repetir(f::codificar(SIETE), 3);
    for (std::size_t corte : {1, 7, 100, 333})
        VERIFICAR(validos(f::leer(desde(bits, corte))) == SIETE);
}

PRUEBA(trama_bit_borrado_cuesta_un_solo_registro) {
    const Bits copia = f::codificar({{TITULO, "uno"}, {ARTISTA, "dos"}, {AUTOR, "tres"}});
    const std::size_t fin_segundo = 2 * f::BITS_SOBRECARGA + 8 * 6;
    Bits danada = copia;
    danada.erase(danada.begin() + static_cast<std::ptrdiff_t>(fin_segundo - 1));
    const auto t = tipos(f::leer(unir({danada, copia})));
    VERIFICAR_IGUAL(contar(t, Tipo::valido, ARTISTA), 1);
    VERIFICAR_IGUAL(contar(t, Tipo::valido, AUTOR), 2);
    VERIFICAR_IGUAL(contar(t, Tipo::valido, TITULO), 2);
}

PRUEBA(trama_bit_insertado_cuesta_un_solo_registro) {
    const Bits copia = f::codificar({{TITULO, "uno"}, {ARTISTA, "dos"}, {AUTOR, "tres"}});
    const Bits danada = unir({Bits(copia.begin(), copia.begin() + 60), Bits{1},
                              Bits(copia.begin() + 60, copia.end())});
    const auto t = tipos(f::leer(unir({danada, copia})));
    VERIFICAR_IGUAL(contar(t, Tipo::valido, TITULO), 1);
    VERIFICAR_IGUAL(contar(t, Tipo::valido, ARTISTA), 2);
}

PRUEBA(trama_falso_preambulo_dentro_del_texto) {
    // "²" es C2 B2 en UTF-8: el B2 seguido de 01 05 parece un registro de Título.
    const std::string texto = "x²\x01\x05ñ";
    VERIFICAR(texto.find("\xB2\x01\x05") != std::string::npos);
    const trama::Metadatos esperado = {{TITULO, texto}, {ARTISTA, "y"}};
    const Bits bits = repetir(f::codificar(esperado), 3);
    for (std::size_t corte : {0, 5, 30}) VERIFICAR(validos(f::leer(desde(bits, corte))) == esperado);
}

PRUEBA(trama_longitud_cero_o_que_no_cabe_no_es_registro) {
    const Bits cero = unir({f::bits_de_bytes(std::string("\xB2\x01\x00\x12\x34", 5)), Bits(40, 0)});
    const Bits no_cabe = unir({f::bits_de_bytes("\xB2\x01\xC8"), Bits(40, 0)});
    VERIFICAR(f::leer(cero).empty());
    VERIFICAR(f::leer(no_cabe).empty());
}

PRUEBA(trama_copia_final_cortada) {
    const Bits copia = f::codificar({{TITULO, "uno"}, {ARTISTA, "dos"}});
    const Bits cortada(copia.begin(), copia.end() - 10);
    const std::vector<std::pair<Tipo, int>> esperado = {
        {Tipo::valido, TITULO}, {Tipo::valido, ARTISTA}, {Tipo::valido, TITULO}};
    VERIFICAR(tipos(f::leer(unir({copia, cortada}))) == esperado);
}

PRUEBA(trama_tipos) {
    const Bits bits = unir({registro(TITULO, "a"), registro(9, "raro"), registro(ARTISTA, "\xff"),
                            registro(AUTOR, "x", 0)});
    const std::vector<Lectura> esperado = {
        {Tipo::valido, TITULO, "a"},
        {Tipo::desconocido, 9, "raro"},
        {Tipo::desconocido, ARTISTA, "\xff"},
        {Tipo::danado, AUTOR, "x"},
    };
    VERIFICAR(f::leer(bits) == esperado);
}

PRUEBA(trama_preambulo_con_un_bit_distinto_en_secuencia) {
    const Bits copia = f::codificar({{TITULO, "a"}, {ARTISTA, "b"}});
    const std::vector<std::pair<Tipo, int>> esperado = {{Tipo::valido, TITULO},
                                                        {Tipo::valido, ARTISTA}};
    for (std::size_t k : {0, 7})  // un bit del preámbulo del primer registro, luego del segundo
        for (std::size_t inicio : {0, 48}) {
            Bits bits = copia;
            bits[inicio + k] ^= 1;
            VERIFICAR(tipos(f::leer(bits)) == esperado);
        }
}

PRUEBA(trama_preambulo_con_dos_bits_distintos_no_se_acepta) {
    Bits bits = f::codificar({{TITULO, "a"}});
    bits[0] ^= 1;
    bits[1] ^= 1;
    VERIFICAR(f::leer(bits).empty());
}

PRUEBA(trama_crc_incorrecto_fuera_de_secuencia_no_se_anota) {
    // Tras un registro dañado, el siguiente no empieza "en secuencia".
    const Bits bits = unir({registro(TITULO, "a", 0), registro(ARTISTA, "b", 0)});
    const std::vector<std::pair<Tipo, int>> esperado = {{Tipo::danado, TITULO}};
    VERIFICAR(tipos(f::leer(bits)) == esperado);
}

PRUEBA(trama_bits_aleatorios_casi_nunca_dan_validos) {
    std::mt19937 rng(3);
    for (int k = 0; k < 20; ++k) {
        Bits bits(1000);
        for (int& b : bits) b = static_cast<int>(rng() & 1);
        for (const auto& l : f::leer(bits)) VERIFICAR(l.tipo != Tipo::valido);
    }
}

PRUEBA(trama_leer_rechaza_bits_que_no_son_0_ni_1) {
    Bits bits = f::codificar(SIETE);
    bits[3] = 2;
    bool lanzo = false;
    try {
        f::leer(bits);
    } catch (const std::invalid_argument&) {
        lanzo = true;
    }
    VERIFICAR(lanzo);
}

// ---- UTF-8 ------------------------------------------------------------------

PRUEBA(trama_utf8_valido) {
    for (const char* bueno : {"", "a", "Ñandú", "\xf0\x9f\x8e\xb5", "\xf4\x8f\xbf\xbf",
                              "\xf3\x80\x80\x80", "\xef\xbf\xbd"})
        VERIFICAR(utf8::valido(bueno));
    // sobrelarga, sustituto, mayor que U+10FFFF, truncada, continuación suelta, F5,
    // sobrelarga de 4 bytes
    for (const char* malo : {"\xc0\xaf", "\xed\xa0\x80", "\xf4\x90\x80\x80", "a\xe2\x82",
                             "\x80", "\xf5\x80\x80\x80", "\xf0\x80\x80\x80"})
        VERIFICAR(!utf8::valido(malo));
}
