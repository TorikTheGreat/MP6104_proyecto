#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

using namespace std;
#include "codificador.hpp"

uint16_t leer_u16(const unsigned char* p) {
    return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}
uint32_t leer_u32(const unsigned char* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}
void escribir_u16(ostream& f, uint16_t v) {
    unsigned char p[2] = {static_cast<unsigned char>(v & 255), static_cast<unsigned char>((v >> 8) & 255)};
    f.write(reinterpret_cast<const char*>(p), 2);
}
void escribir_u32(ostream& f, uint32_t v) {
    unsigned char p[4] = {static_cast<unsigned char>(v & 255), static_cast<unsigned char>((v >> 8) & 255),
                          static_cast<unsigned char>((v >> 16) & 255), static_cast<unsigned char>((v >> 24) & 255)};
    f.write(reinterpret_cast<const char*>(p), 4);
}

Audio leer_wav(const string& ruta) {
    ifstream f(ruta, ios::binary);
    if (!f) throw runtime_error("No se pudo abrir: " + ruta);
    unsigned char cab[12];
    if (!f.read(reinterpret_cast<char*>(cab), 12) || memcmp(cab, "RIFF", 4) || memcmp(cab + 8, "WAVE", 4))
        throw runtime_error("El archivo no es RIFF/WAVE valido");
    bool hay_fmt = false, hay_data = false;
    uint16_t formato = 0, canales = 0, bits = 0, alineacion = 0;
    uint32_t fs = 0, tamano_data = 0;
    streampos posicion_data = streampos(-1);
    while (true) {
        unsigned char bloque[8];
        if (!f.read(reinterpret_cast<char*>(bloque), 8)) break;
        uint32_t tamano = leer_u32(bloque + 4);
        streampos inicio = f.tellg();
        if (memcmp(bloque, "fmt ", 4) == 0) {
            if (tamano < 16) throw runtime_error("Bloque fmt demasiado corto");
            unsigned char fmt[16];
            if (!f.read(reinterpret_cast<char*>(fmt), 16)) throw runtime_error("No se pudo leer fmt");
            formato = leer_u16(fmt);
            canales = leer_u16(fmt + 2);
            fs = leer_u32(fmt + 4);
            alineacion = leer_u16(fmt + 12);
            bits = leer_u16(fmt + 14);
            hay_fmt = true;
        } else if (memcmp(bloque, "data", 4) == 0) {
            tamano_data = tamano;
            posicion_data = inicio;
            hay_data = true;
        }
        // Saltar al siguiente bloque, incluyendo el byte de relleno si existe.
        f.seekg(inicio + static_cast<streamoff>(tamano) + static_cast<streamoff>(tamano & 1u));
        if (!f) throw runtime_error("Bloque WAV incompleto o desplazamiento invalido");
        if (hay_fmt && hay_data) break;
    }
    if (!hay_fmt || !hay_data) throw runtime_error("Falta bloque fmt o data");
    if (formato != 1 || canales != 2 || fs != 44100 || bits != 16 || alineacion != 4)
        throw runtime_error("Se requiere WAV PCM 16 bits, estereo, 44100 Hz");
    if (tamano_data == 0 || tamano_data % 4 != 0)
        throw runtime_error("Tamano del bloque data invalido");
    const size_t n = tamano_data / 4;
    Audio audio;
    audio.fs = fs;
    audio.izquierdo.resize(n);
    audio.derecho.resize(n);
    f.clear();
    f.seekg(posicion_data);
    for (size_t i = 0; i < n; ++i) {
        unsigned char p[4];
        if (!f.read(reinterpret_cast<char*>(p), 4)) throw runtime_error("Archivo de audio truncado");
        uint16_t l = leer_u16(p), r = leer_u16(p + 2);
        audio.izquierdo[i] = static_cast<int16_t>(l < 32768 ? static_cast<int32_t>(l) : static_cast<int32_t>(l) - 65536);
        audio.derecho[i] = static_cast<int16_t>(r < 32768 ? static_cast<int32_t>(r) : static_cast<int32_t>(r) - 65536);
    }
    return audio;
}

int16_t pcm16(double valor, size_t& recortes) {
    if (valor > 32767.0 || valor < -32768.0) ++recortes;
    valor = clamp(valor, -32768.0, 32767.0);
    // Equivale a convertir el float a int16 en el prototipo Python (truncamiento hacia cero).
    return static_cast<int16_t>(valor);
}

void escribir_wav(const string& ruta, const Audio& original, const vector<double>& codificado,
                 bool canal_izquierdo, size_t& recortes) {
    const size_t n = codificado.size();
    if (n > (numeric_limits<uint32_t>::max() - 36u) / 4u)
        throw runtime_error("Audio demasiado grande para WAV RIFF clasico");
    const uint32_t datos_bytes = static_cast<uint32_t>(n * 4u);
    ofstream f(ruta, ios::binary);
    if (!f) throw runtime_error("No se pudo crear: " + ruta);
    f.write("RIFF", 4);
    escribir_u32(f, 36u + datos_bytes);
    f.write("WAVE", 4);
    f.write("fmt ", 4);
    escribir_u32(f, 16);
    escribir_u16(f, 1);
    escribir_u16(f, 2);
    escribir_u32(f, original.fs);
    escribir_u32(f, original.fs * 4);
    escribir_u16(f, 4);
    escribir_u16(f, 16);
    f.write("data", 4);
    escribir_u32(f, datos_bytes);
    for (size_t i = 0; i < n; ++i) {
        int16_t nuevo = pcm16(codificado[i], recortes);
        int16_t sin_cambios = 0;
        if (i < original.izquierdo.size())
            sin_cambios = canal_izquierdo ? original.derecho[i] : original.izquierdo[i];
        int16_t l = canal_izquierdo ? nuevo : sin_cambios;
        int16_t r = canal_izquierdo ? sin_cambios : nuevo;
        escribir_u16(f, static_cast<uint16_t>(l));
        escribir_u16(f, static_cast<uint16_t>(r));
    }
    if (!f) throw runtime_error("Error escribiendo WAV de salida");
}


vector<double> codificar_eco(const vector<int16_t>& x, const string& bits,
                            size_t D0, size_t D1, double a0, double a1,
                            size_t ventana) {
    const size_t maxD = max(D0, D1);

    if (D0 == 0 || D1 == 0 || D0 == D1 ||
        ventana <= maxD || ventana > x.size())
        throw runtime_error("Retardos o ventana invalidos");

    if (!isfinite(a0) || !isfinite(a1) ||
        a0 < 0 || a0 >= 1 || a1 < 0 || a1 >= 1)
        throw runtime_error("Atenuaciones invalidas");

    if (bits.empty() || bits.size() > x.size() / ventana ||
        bits.find_first_not_of("01") != string::npos)
        throw runtime_error("Mensaje binario invalido o demasiado largo");

    // Salida en ceros, con espacio para la cola de la ultima convolucion.
    vector<double> salida(x.size() + maxD, 0.0);

    for (size_t b = 0; b < bits.size(); ++b) {
        const size_t D = bits[b] == '0' ? D0 : D1;
        const double a = bits[b] == '0' ? a0 : a1;
        const size_t inicio = b * ventana;

        // Respuesta al impulso: h[n] = delta[n] + a*delta[n-D].
        vector<double> h(D + 1, 0.0);
        h[0] = 1.0;
        h[D] = a;

        // Convolucion lineal de longitud M + N - 1.
        vector<double> y(ventana + h.size() - 1, 0.0);

        for (size_t n = 0; n < ventana; ++n) {
            const double muestra = static_cast<double>(x[inicio + n]);
            for (size_t k = 0; k < h.size(); ++k)
                y[n + k] += muestra * h[k];
        }

        // Solapamiento y suma: la cola de cada ventana se suma a la siguiente.
        for (size_t n = 0; n < y.size(); ++n)
            salida[inicio + n] += y[n];
    }

    // El audio despues de la ultima ventana con bit se copia sin cambios.
    for (size_t i = bits.size() * ventana; i < x.size(); ++i)
        salida[i] += static_cast<double>(x[i]);

    return salida;
}
