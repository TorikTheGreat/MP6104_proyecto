#include "notificacion.hpp"

#include <iomanip>
#include <locale>
#include <sstream>
#include <stdexcept>

#include "parametros.hpp"
#include "utf8.hpp"

namespace notificacion {

using trama::Tipo;

std::string texto_visible(const std::string& datos) {
    std::string texto;
    for (std::size_t i = 0; i < datos.size();) {
        const std::size_t largo = utf8::largo_secuencia(datos, i);
        if (largo > 0) {
            texto += datos.substr(i, largo);
            i += largo;
        } else {
            texto += '?';
            i += 1;
        }
    }
    return texto;
}

std::pair<std::vector<std::string>, std::vector<std::string>> notificar(
    const std::vector<trama::Lectura>& resultados) {
    std::vector<std::string> salida;
    std::vector<std::string> errores;
    for (const auto& r : resultados) {
        const std::string texto = texto_visible(r.datos);
        if (r.tipo == Tipo::valido) {
            const char* nombre = parametros::nombre_categoria(r.categoria);
            if (nombre == nullptr)
                throw std::invalid_argument("lectura válida con categoría inválida: " +
                                            std::to_string(r.categoria));
            salida.push_back(std::string(nombre) + ": " + texto);
        } else if (r.tipo == Tipo::danado) {
            // Sin categoría: el byte de categoría está bajo el CRC que falló.
            errores.push_back("ADVERTENCIA: metadato desconocido (dañado): " + texto);
        } else {
            errores.push_back("ADVERTENCIA: metadato desconocido (categoría " +
                              std::to_string(r.categoria) + "): " + texto);
        }
    }
    if (resultados.empty()) errores.push_back("ADVERTENCIA: no se encontraron metadatos");
    return {salida, errores};
}

std::vector<std::string> aviso_no_cabe(std::int64_t necesarios, std::int64_t disponibles,
                                       std::int64_t largo_ventana) {
    const double faltan_s =
        static_cast<double>((necesarios - disponibles) * largo_ventana) / parametros::FS;
    // Locale clásica: siempre punto decimal, como Python, aunque main cambie la locale.
    std::ostringstream segundos;
    segundos.imbue(std::locale::classic());
    segundos << std::fixed << std::setprecision(1) << faltan_s;
    const std::string borde(72, '*');
    return {
        borde,
        "ADVERTENCIA: los metadatos no caben en el audio.",
        "Se codificaron " + std::to_string(disponibles) + " de " + std::to_string(necesarios) +
            " bits; el resto se perdió.",
        "Para codificarlos completos faltan unos " + segundos.str() + " s de audio.",
        borde,
    };
}

} 
