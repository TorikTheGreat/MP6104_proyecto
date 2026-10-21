#include "aplicacion.hpp"

#include <algorithm>
#include <exception>
#include <fstream>
#include <iomanip>
#include <locale>
#include <stdexcept>
#include <variant>

#include "linea_comandos.hpp"
#include "metadatos.hpp"
#include "notificacion.hpp"
#include "parametros.hpp"

namespace aplicacion {

namespace lc = linea_comandos;

namespace {

bool hay_sonido(const std::vector<std::int16_t>& canal, std::size_t inicio, std::size_t largo) {
    for (std::size_t i = inicio; i < inicio + largo; ++i)
        if (canal[i] != 0) return true;
    return false;
}

// La misma conversión a 16 bits que el escritor de WAV: saturar y truncar.
std::vector<std::int16_t> a_16_bits(const std::vector<double>& y) {
    std::vector<std::int16_t> z(y.size());
    for (std::size_t i = 0; i < y.size(); ++i)
        z[i] = static_cast<std::int16_t>(std::clamp(y[i], -32768.0, 32767.0));
    return z;
}

std::ofstream abrir_csv(const std::string& ruta) {
    std::ofstream f(ruta);
    if (!f) throw std::runtime_error("no se pudo escribir " + ruta);
    f.imbue(std::locale::classic());
    return f;
}

// --diagnostico al codificar: el bit de cada ventana (-1 si va sin eco).
void diagnostico_codificacion(const std::string& ruta, const std::vector<int>& bits) {
    std::ofstream f = abrir_csv(ruta);
    f << "ventana,bit\n";
    for (std::size_t k = 0; k < bits.size(); ++k) f << k << ',' << bits[k] << '\n';
    if (!f) throw std::runtime_error("no se pudo escribir " + ruta);
}

// --diagnostico al decodificar: la clasificación de cada ventana (A29).
void diagnostico_decodificacion(const std::string& ruta, const std::vector<Ventana>& ventanas) {
    std::ofstream f = abrir_csv(ruta);
    f << std::setprecision(17) << "ventana,bit,r0,r1,rms\n";
    for (const auto& v : ventanas)
        f << v.indice << ',' << v.bit << ',' << v.r0 << ',' << v.r1 << ',' << v.rms << '\n';
    if (!f) throw std::runtime_error("no se pudo escribir " + ruta);
}

Eco eco_desde(const lc::Configuracion& c) {
    const lc::Enmascaramiento m = lc::resolver(c);
    return {static_cast<std::size_t>(parametros::muestras_desde_us(m.retardo0_us)),
            static_cast<std::size_t>(parametros::muestras_desde_us(m.retardo1_us)),
            static_cast<std::size_t>(parametros::largo_ventana(m.retardo0_us, m.retardo1_us)),
            parametros::amplitud_desde_porcentaje(m.atenuacion0),
            parametros::amplitud_desde_porcentaje(m.atenuacion1)};
}

int codificar(const lc::Configuracion& c, const Audio& audio, bool izquierdo, const Eco& eco,
              std::ostream& error, const Bloques& bloques) {
    const auto& canal = izquierdo ? audio.izquierdo : audio.derecho;
    const auto lleva_bit = ventanas_con_bit(canal, c.metadatos, eco, bloques);
    const auto disponibles =
        static_cast<std::size_t>(std::count(lleva_bit.begin(), lleva_bit.end(), true));
    const metadatos::Codificacion cod = metadatos::a_bits(c.metadatos, disponibles);
    const std::vector<int> bits = bits_por_ventana(cod.bits, lleva_bit);

    const std::vector<double> codificado =
        cod.bits.empty() ? std::vector<double>(canal.begin(), canal.end())
                         : bloques.codificar(canal, bits, eco);
    const std::size_t recortes = bloques.escribir(c.salida, audio, codificado, izquierdo);

    if (cod.truncado)
        for (const auto& linea : notificacion::aviso_no_cabe(
                 static_cast<std::int64_t>(cod.necesarios), static_cast<std::int64_t>(cod.bits.size()),
                 static_cast<std::int64_t>(eco.largo_ventana)))
            error << linea << '\n';
    if (recortes > 0)
        error << "ADVERTENCIA: " << recortes << " muestras se saturaron al escribir el audio\n";
    if (c.diagnostico) diagnostico_codificacion(*c.diagnostico, bits);
    return lc::EXITO;
}

int decodificar(const lc::Configuracion& c, const Audio& audio, bool izquierdo, const Eco& eco,
                std::ostream& salida, std::ostream& error, const Bloques& bloques) {
    const Decodificacion d = bloques.decodificar(izquierdo ? audio.izquierdo : audio.derecho, eco);
    const auto [lineas, avisos] = notificacion::notificar(metadatos::desde_bits(d.bits));
    for (const auto& l : lineas) salida << l << '\n';
    for (const auto& l : avisos) error << l << '\n';
    if (c.diagnostico) diagnostico_decodificacion(*c.diagnostico, d.ventanas);
    return lc::EXITO;
}

}  // namespace

std::vector<int> bits_por_ventana(const std::vector<int>& mensaje,
                                  const std::vector<bool>& lleva_bit) {
    std::vector<int> bits(lleva_bit.size(), -1);
    std::size_t j = 0;
    for (std::size_t k = 0; k < lleva_bit.size() && j < mensaje.size(); ++k)
        if (lleva_bit[k]) bits[k] = mensaje[j++];
    return bits;
}

std::vector<bool> ventanas_con_bit(const std::vector<std::int16_t>& canal,
                                   const trama::Metadatos& metadatos, const Eco& eco,
                                   const Bloques& bloques) {
    const std::size_t largo = eco.largo_ventana;
    const std::size_t n = canal.size() / largo;
    std::vector<bool> lleva(n);
    for (std::size_t k = 0; k < n; ++k) lleva[k] = hay_sonido(canal, k * largo, largo);
    if (std::all_of(lleva.begin(), lleva.end(), [](bool b) { return b; })) return lleva;

    for (int pasada = 0; pasada < 4; ++pasada) {
        const auto disponibles = static_cast<std::size_t>(std::count(lleva.begin(), lleva.end(), true));
        const auto mensaje = metadatos::a_bits(metadatos, disponibles).bits;
        if (mensaje.empty()) return lleva;
        const auto codificado = a_16_bits(bloques.codificar(canal, bits_por_ventana(mensaje, lleva), eco));
        std::vector<bool> nuevo(n);
        for (std::size_t k = 0; k < n; ++k) nuevo[k] = hay_sonido(codificado, k * largo, largo);
        if (nuevo == lleva) break;
        lleva = nuevo;
    }
    return lleva;
}

int ejecutar(const std::vector<std::string>& args, std::ostream& salida, std::ostream& error,
             const Bloques& bloques) {
    const auto analisis = lc::analizar(args);
    if (const auto* e = std::get_if<lc::ErrorUso>(&analisis)) {
        error << "error: " << e->mensaje << "\n\n" << lc::uso();
        return lc::ERROR_USO;
    }
    const auto& c = std::get<lc::Configuracion>(analisis);
    if (c.modo == lc::Modo::ayuda) {
        salida << lc::uso();
        return lc::EXITO;
    }
    try {
        const Eco eco = eco_desde(c);
        const Audio audio = bloques.leer(c.entrada);
        const bool izquierdo = c.canal == "izquierdo";
        if (c.modo == lc::Modo::codificar) return codificar(c, audio, izquierdo, eco, error, bloques);
        return decodificar(c, audio, izquierdo, eco, salida, error, bloques);
    } catch (const std::exception& e) {
        error << "error: " << e.what() << '\n';
        return lc::ERROR_EJECUCION;
    }
}

}  // namespace aplicacion
