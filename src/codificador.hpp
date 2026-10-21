#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

struct Audio {
    uint32_t fs = 0;
    std::vector<int16_t> izquierdo, derecho;
};

Audio leer_wav(const std::string& ruta);
void escribir_wav(const std::string& ruta, const Audio& original,
                 const std::vector<double>& codificado,
                 bool canal_izquierdo, size_t& recortes);
std::vector<double> codificar_eco(const std::vector<int16_t>& x,
                                 const std::string& bits, size_t D0, size_t D1,
                                 double a0, double a1, size_t ventana);
