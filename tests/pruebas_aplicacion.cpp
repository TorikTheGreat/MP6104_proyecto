// Pruebas de src/aplicacion.*: el flujo de ecoesteg con bloques falsos (sin audio),
// y una de punta a punta con los bloques reales sobre un WAV sintético.

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "aplicacion.hpp"
#include "linea_comandos.hpp"
#include "metadatos.hpp"
#include "parametros.hpp"
#include "pruebas.hpp"

using aplicacion::Audio;
using aplicacion::Bloques;
using aplicacion::Eco;
namespace lc = linea_comandos;

namespace {

constexpr std::size_t LARGO = 25 * 66;  // la ventana con los valores por defecto

// Lo que los bloques falsos recibieron y lo que devuelven.
struct Registro {
    std::map<std::string, Audio> archivos;  // "disco": lo que se lee y se escribe
    std::vector<int> bits_codificados;
    std::vector<std::int16_t> canal_codificado;
    bool escribio_izquierdo = false;
    std::vector<int> bits_a_decodificar;
    std::vector<std::int16_t> canal_decodificado;
};

Bloques falsos(const std::shared_ptr<Registro>& r) {
    Bloques b;
    b.leer = [r](const std::string& ruta) {
        const auto it = r->archivos.find(ruta);
        if (it == r->archivos.end()) throw std::runtime_error("No se pudo abrir: " + ruta);
        return it->second;
    };
    b.escribir = [r](const std::string& ruta, const Audio& original,
                     const std::vector<double>& codificado, bool izquierdo) {
        Audio a = original;
        auto& canal = izquierdo ? a.izquierdo : a.derecho;
        for (std::size_t i = 0; i < canal.size(); ++i) canal[i] = static_cast<std::int16_t>(codificado[i]);
        r->archivos[ruta] = a;
        r->escribio_izquierdo = izquierdo;
        return std::size_t{0};
    };
    b.codificar = [r](const std::vector<std::int16_t>& canal, const std::vector<int>& bits, const Eco&) {
        r->bits_codificados = bits;
        r->canal_codificado = canal;
        return std::vector<double>(canal.begin(), canal.end());
    };
    b.decodificar = [r](const std::vector<std::int16_t>& canal, const Eco&) {
        r->canal_decodificado = canal;
        aplicacion::Decodificacion d;
        d.bits = r->bits_a_decodificar;
        for (std::size_t k = 0; k < d.bits.size(); ++k)
            d.ventanas.push_back({k, d.bits[k], d.bits[k] == 0 ? 1.0 : 0.0, d.bits[k] == 1 ? 1.0 : 0.0, 1.0});
        return d;
    };
    return b;
}

Audio audio_con_sonido(std::size_t ventanas, std::int16_t izq = 100, std::int16_t der = -7) {
    return {std::vector<std::int16_t>(ventanas * LARGO, izq),
            std::vector<std::int16_t>(ventanas * LARGO, der)};
}

struct Corrida {
    int codigo;
    std::string salida;
    std::string error;
};

Corrida correr(const std::vector<std::string>& args, const Bloques& b) {
    std::ostringstream salida, error;
    const int codigo = aplicacion::ejecutar(args, salida, error, b);
    return {codigo, salida.str(), error.str()};
}

bool contiene(const std::string& texto, const std::string& parte) {
    return texto.find(parte) != std::string::npos;
}

std::string leer_archivo(const std::string& ruta) {
    std::ifstream f(ruta);
    std::stringstream s;
    s << f.rdbuf();
    return s.str();
}

std::string temporal(const std::string& nombre) {
    return (std::filesystem::temp_directory_path() / ("ecoesteg_prueba_" + nombre)).string();
}

}  // namespace

// -------- Modos y códigos de salida --------------
PRUEBA(aplicacion_error_de_uso) {
    const auto r = correr({}, falsos(std::make_shared<Registro>()));
    VERIFICAR_IGUAL(r.codigo, lc::ERROR_USO);
    VERIFICAR(r.error.rfind("error: ", 0) == 0);
    VERIFICAR(contiene(r.error, "Uso:"));
    VERIFICAR(r.salida.empty());
}

PRUEBA(aplicacion_ayuda) {
    const auto r = correr({"--ayuda"}, falsos(std::make_shared<Registro>()));
    VERIFICAR_IGUAL(r.codigo, lc::EXITO);
    VERIFICAR_IGUAL(r.salida, lc::uso());
}

PRUEBA(aplicacion_archivo_que_no_se_puede_leer) {
    const auto r = correr({"no_existe.wav"}, falsos(std::make_shared<Registro>()));
    VERIFICAR_IGUAL(r.codigo, lc::ERROR_EJECUCION);
    VERIFICAR(contiene(r.error, "No se pudo abrir: no_existe.wav"));
}

// -------- Codificar ------
PRUEBA(aplicacion_codifica_el_mensaje_repetido_en_todas_las_ventanas) {
    auto reg = std::make_shared<Registro>();
    reg->archivos["e.wav"] = audio_con_sonido(300);
    const auto r = correr({"e.wav", "s.wav", "--titulo", "Hola"}, falsos(reg));
    VERIFICAR_IGUAL(r.codigo, lc::EXITO);
    VERIFICAR(r.error.empty());
    VERIFICAR(reg->archivos.count("s.wav") == 1);
    VERIFICAR(reg->escribio_izquierdo);
    VERIFICAR_IGUAL(reg->bits_codificados, metadatos::a_bits({{parametros::TITULO, "Hola"}}, 300).bits);
}

PRUEBA(aplicacion_canal_derecho) {
    auto reg = std::make_shared<Registro>();
    reg->archivos["e.wav"] = audio_con_sonido(300, 100, -7);
    const auto r = correr({"e.wav", "s.wav", "--titulo", "Hola", "--canal", "derecho"}, falsos(reg));
    VERIFICAR_IGUAL(r.codigo, lc::EXITO);
    VERIFICAR(!reg->escribio_izquierdo);
    REQUERIR(!reg->canal_codificado.empty());
    VERIFICAR_IGUAL(reg->canal_codificado[0], -7);
}

PRUEBA(aplicacion_mensaje_que_no_cabe_se_trunca_y_avisa) {
    auto reg = std::make_shared<Registro>();
    reg->archivos["e.wav"] = audio_con_sonido(30);  // 30 bits; "Hola" necesita 72
    const auto r = correr({"e.wav", "s.wav", "--titulo", "Hola"}, falsos(reg));
    VERIFICAR_IGUAL(r.codigo, lc::EXITO);
    VERIFICAR(contiene(r.error, "Se codificaron 30 de 72 bits"));
    VERIFICAR(reg->archivos.count("s.wav") == 1);
}

PRUEBA(aplicacion_audio_sin_ninguna_ventana) {
    auto reg = std::make_shared<Registro>();
    reg->archivos["e.wav"] = {std::vector<std::int16_t>(100, 5), std::vector<std::int16_t>(100, 5)};
    const auto r = correr({"e.wav", "s.wav", "--titulo", "Hola"}, falsos(reg));
    VERIFICAR_IGUAL(r.codigo, lc::EXITO);
    VERIFICAR(contiene(r.error, "Se codificaron 0 de 72 bits"));
    VERIFICAR(reg->bits_codificados.empty());  // no se llamó al codificador
    VERIFICAR(reg->archivos.count("s.wav") == 1);
}

PRUEBA(aplicacion_ventanas_en_silencio_no_llevan_bit) {
    auto reg = std::make_shared<Registro>();
    Audio a = audio_con_sonido(300);
    for (std::size_t i = 5 * LARGO; i < 6 * LARGO; ++i) a.izquierdo[i] = 0;  // ventana 5
    reg->archivos["e.wav"] = a;
    VERIFICAR_IGUAL(correr({"e.wav", "s.wav", "--titulo", "Hola"}, falsos(reg)).codigo, lc::EXITO);
    const auto mensaje = metadatos::a_bits({{parametros::TITULO, "Hola"}}, 299).bits;
    REQUERIR(reg->bits_codificados.size() == 300);
    VERIFICAR_IGUAL(reg->bits_codificados[5], -1);
    VERIFICAR_IGUAL(reg->bits_codificados[4], mensaje[4]);
    VERIFICAR_IGUAL(reg->bits_codificados[6], mensaje[5]);  // el mensaje sigue en la 6
}

PRUEBA(aplicacion_diagnostico_al_codificar) {
    auto reg = std::make_shared<Registro>();
    reg->archivos["e.wav"] = audio_con_sonido(100);
    const std::string csv = temporal("codificar.csv");
    VERIFICAR_IGUAL(correr({"e.wav", "s.wav", "--titulo", "Hola", "--diagnostico", csv}, falsos(reg)).codigo,
                    lc::EXITO);
    const std::string texto = leer_archivo(csv);
    VERIFICAR(texto.rfind("ventana,bit\n0,1\n", 0) == 0);  // el preámbulo 0xB2 empieza con 1
    std::remove(csv.c_str());
}

// ------- Decodificar -----------
PRUEBA(aplicacion_decodifica_y_notifica) {
    auto reg = std::make_shared<Registro>();
    reg->archivos["c.wav"] = audio_con_sonido(10);
    reg->bits_a_decodificar =
        metadatos::a_bits({{parametros::TITULO, "Hola"}, {parametros::ANIO, "2026"}}, 400).bits;
    const auto r = correr({"c.wav"}, falsos(reg));
    VERIFICAR_IGUAL(r.codigo, lc::EXITO);
    VERIFICAR_IGUAL(r.salida, "Título: Hola\nAño: 2026\n");
    VERIFICAR(r.error.empty());
}

PRUEBA(aplicacion_decodifica_el_canal_pedido) {
    auto reg = std::make_shared<Registro>();
    reg->archivos["c.wav"] = audio_con_sonido(10, 100, -7);
    correr({"c.wav", "--canal", "derecho"}, falsos(reg));
    REQUERIR(!reg->canal_decodificado.empty());
    VERIFICAR_IGUAL(reg->canal_decodificado[0], -7);
}

PRUEBA(aplicacion_sin_metadatos) {
    auto reg = std::make_shared<Registro>();
    reg->archivos["c.wav"] = audio_con_sonido(10);
    const auto r = correr({"c.wav"}, falsos(reg));
    VERIFICAR_IGUAL(r.codigo, lc::EXITO);
    VERIFICAR(r.salida.empty());
    VERIFICAR_IGUAL(r.error, "ADVERTENCIA: no se encontraron metadatos\n");
}

PRUEBA(aplicacion_diagnostico_al_decodificar) {
    auto reg = std::make_shared<Registro>();
    reg->archivos["c.wav"] = audio_con_sonido(10);
    reg->bits_a_decodificar = {1, 0};
    const std::string csv = temporal("decodificar.csv");
    correr({"c.wav", "--diagnostico", csv}, falsos(reg));
    VERIFICAR_IGUAL(leer_archivo(csv), "ventana,bit,r0,r1,rms\n0,1,0,1,1\n1,0,1,0,1\n");
    std::remove(csv.c_str());
}

PRUEBA(aplicacion_diagnostico_no_puede_ser_la_entrada) {
    auto reg = std::make_shared<Registro>();
    reg->archivos["c.wav"] = audio_con_sonido(10);
    const auto r = correr({"c.wav", "--diagnostico", "c.wav"}, falsos(reg));
    VERIFICAR_IGUAL(r.codigo, lc::ERROR_EJECUCION);
    VERIFICAR(contiene(r.error, "--diagnostico no puede ser"));
}

PRUEBA(aplicacion_diagnostico_en_ruta_invalida) {
    auto reg = std::make_shared<Registro>();
    reg->archivos["c.wav"] = audio_con_sonido(10);
    const auto r = correr({"c.wav", "--diagnostico", "/no/existe/d.csv"}, falsos(reg));
    VERIFICAR_IGUAL(r.codigo, lc::ERROR_EJECUCION);
}

// ------ De punta a punta, con los bloques reales ----

namespace {

// El mismo generador que ruido() en prototipo/vectores_aplicacion.py.
std::vector<std::int16_t> ruido(std::size_t n, std::uint64_t x) {
    std::vector<std::int16_t> muestras(n);
    for (auto& m : muestras) {
        x = (1103515245u * x + 12345u) % (std::uint64_t{1} << 31);
        m = static_cast<std::int16_t>(static_cast<long long>((x >> 8) % 20001) - 10000);
    }
    return muestras;
}

}  // namespace

PRUEBA(aplicacion_igual_que_python) {
    // Los mismos argumentos que prototipo/vectores_aplicacion.py.
    const std::size_t n = 80 * 175;
    const auto muestras = ruido(2 * n, 2026);
    const Audio a{std::vector<std::int16_t>(muestras.begin(), muestras.begin() + n),
                  std::vector<std::int16_t>(muestras.begin() + n, muestras.end())};
    const Bloques reales = aplicacion::bloques_reales();
    const std::string original = temporal("paridad_original.wav");
    const std::string codificado = temporal("paridad_codificado.wav");
    reales.escribir(original, a, std::vector<double>(a.izquierdo.begin(), a.izquierdo.end()), true);
    const std::vector<std::string> eco = {"--retardo0", "100", "--retardo1", "150",
                                          "--atenuacion0", "30", "--atenuacion1", "30"};

    std::vector<std::string> args = {original, codificado, "--titulo", "Hola"};
    args.insert(args.end(), eco.begin(), eco.end());
    VERIFICAR_IGUAL(correr(args, reales).codigo, lc::EXITO);
    VERIFICAR_IGUAL(reales.leer(codificado).izquierdo, pruebas::leer_enteros("aplicacion_codificado"));

    args = {codificado};
    args.insert(args.end(), eco.begin(), eco.end());
    const auto r = correr(args, reales);
    std::string salida, errores;
    for (const auto& l : pruebas::leer_textos("aplicacion_salida")) salida += l + "\n";
    for (const auto& l : pruebas::leer_textos("aplicacion_errores")) errores += l + "\n";
    VERIFICAR_IGUAL(r.salida, salida);
    VERIFICAR_IGUAL(r.error, errores);
    std::remove(original.c_str());
    std::remove(codificado.c_str());
}

PRUEBA(aplicacion_punta_a_punta_con_wav_sintetico) {
    // 20 s de ruido en los dos canales: el decodificador lo lee casi sin errores.
    const std::size_t n = 20 * parametros::FS;
    std::mt19937 rng(11);
    std::normal_distribution<double> ruido(0.0, 3000.0);
    Audio a{std::vector<std::int16_t>(n), std::vector<std::int16_t>(n)};
    for (std::size_t i = 0; i < n; ++i) {
        a.izquierdo[i] = static_cast<std::int16_t>(std::clamp(ruido(rng), -32768.0, 32767.0));
        a.derecho[i] = static_cast<std::int16_t>(std::clamp(ruido(rng), -32768.0, 32767.0));
    }
    const Bloques reales = aplicacion::bloques_reales();
    const std::string original = temporal("original.wav");
    const std::string codificado = temporal("codificado.wav");
    reales.escribir(original, a, std::vector<double>(a.izquierdo.begin(), a.izquierdo.end()), true);

    const auto cod = correr({original, codificado, "--titulo", "Canción", "--anio", "2026"}, reales);
    VERIFICAR_IGUAL(cod.codigo, lc::EXITO);
    const auto dec = correr({codificado}, reales);
    VERIFICAR_IGUAL(dec.codigo, lc::EXITO);
    VERIFICAR_IGUAL(dec.salida, "Título: Canción\nAño: 2026\n");
    // El otro canal queda como estaba (R26): ahí no hay nada.
    VERIFICAR(correr({codificado, "--canal", "derecho"}, reales).salida.empty());
    // Un archivo sin codificar no da metadatos.
    VERIFICAR(correr({original}, reales).salida.empty());
    // La misma salida que la entrada, escrita distinto: no se pisa el original.
    const std::string antes = leer_archivo(original);
    const std::string otra_forma =
        (std::filesystem::path(original).parent_path() / "." /
         std::filesystem::path(original).filename()).string();
    const auto r = correr({original, otra_forma, "--titulo", "x"}, reales);
    VERIFICAR_IGUAL(r.codigo, lc::ERROR_EJECUCION);
    VERIFICAR(contiene(r.error, "mismo archivo"));
    VERIFICAR(leer_archivo(original) == antes);
    std::remove(original.c_str());
    std::remove(codificado.c_str());
}

PRUEBA(aplicacion_wav_extensible_y_tamano_de_datos_de_mas) {
    // WAVE_FORMAT_EXTENSIBLE con PCM, y un bloque data que dice ser más largo que el
    // archivo (como la salida de ffmpeg por tubería).
    std::string fmt;
    const auto u16 = [](std::string& s, unsigned v) { s += static_cast<char>(v & 255); s += static_cast<char>(v >> 8); };
    const auto u32 = [&](std::string& s, unsigned long v) { u16(s, v & 0xFFFF); u16(s, (v >> 16) & 0xFFFF); };
    u16(fmt, 0xFFFE); u16(fmt, 2); u32(fmt, 44100); u32(fmt, 44100 * 4); u16(fmt, 4); u16(fmt, 16);
    u16(fmt, 22); u16(fmt, 16); u32(fmt, 3);
    fmt += std::string("\x01\x00\x00\x00\x00\x00\x10\x00\x80\x00\x00\xaa\x00\x38\x9b\x71", 16);
    std::string datos;
    for (int m = -50; m < 50; ++m) u16(datos, static_cast<unsigned>(m) & 0xFFFF);
    std::string wav = "RIFF";
    u32(wav, 0xFFFFFFFF);
    wav += "WAVEfmt ";
    u32(wav, fmt.size());
    wav += fmt + "data";
    u32(wav, 0xFFFFFFFF);
    wav += datos + std::string(1, '\x01');
    const std::string ruta = temporal("extensible.wav");
    std::ofstream(ruta, std::ios::binary) << wav;
    const Audio a = aplicacion::bloques_reales().leer(ruta);
    REQUERIR(a.izquierdo.size() == 50);
    VERIFICAR_IGUAL(a.izquierdo[0], -50);
    VERIFICAR_IGUAL(a.derecho[0], -49);
    VERIFICAR_IGUAL(a.derecho[49], 49);
    std::remove(ruta.c_str());
}
