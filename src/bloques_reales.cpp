#include "bloques_reales.hpp"

#include <string>
#include <utility>
#include <vector>

#include "codificador.hpp"
#include "decodificador.hpp"
#include "parametros.hpp"

namespace aplicacion {

Bloques bloques_reales() {
    Bloques b;
    b.leer = [](const std::string& ruta) {
        ::Audio a = leer_wav(ruta);
        return Audio{std::move(a.izquierdo), std::move(a.derecho)};
    };
    b.escribir = [](const std::string& ruta, const Audio& original,
                    const std::vector<double>& codificado, bool izquierdo) {
        ::Audio a;
        a.fs = parametros::FS;
        a.izquierdo = original.izquierdo;
        a.derecho = original.derecho;
        std::size_t recortes = 0;
        escribir_wav(ruta, a, codificado, izquierdo, recortes);
        return recortes;
    };
    b.codificar = [](const std::vector<std::int16_t>& canal, const std::vector<int>& bits,
                     const Eco& eco) {
        // codificar_eco codifica las primeras ventanas, una por carácter, y deja
        // sin eco las que siguen. Las ventanas sin bit del final se omiten; las del
        // medio son silencio digital, y el eco de un silencio es silencio.
        std::size_t usadas = bits.size();
        while (usadas > 0 && bits[usadas - 1] < 0) --usadas;
        if (usadas == 0) return std::vector<double>(canal.begin(), canal.end());
        std::string cadena(usadas, '0');
        for (std::size_t k = 0; k < usadas; ++k)
            if (bits[k] == 1) cadena[k] = '1';
        return codificar_eco(canal, cadena, eco.d0, eco.d1, eco.a0, eco.a1, eco.largo_ventana);
    };
    b.decodificar = [](const std::vector<std::int16_t>& canal, const Eco& eco) {
        const ResultadoDecodificacion r = decodificar_senal(canal, eco.d0, eco.d1, eco.largo_ventana);
        Decodificacion d;
        d.bits = r.bits;
        for (const auto& v : r.ventanas) d.ventanas.push_back({v.indice, v.bit, v.r0, v.r1, v.rms});
        return d;
    };
    return b;
}

}  // namespace aplicacion
