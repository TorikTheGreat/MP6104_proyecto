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
struct Audio {
    uint32_t fs = 0;
    vector<int16_t> izquierdo, derecho;
};

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

string pedir_texto(const string& etiqueta, const string& defecto) {
    cout << etiqueta << " [" << defecto << "]: ";
    string s;
    if (!getline(cin, s)) throw runtime_error("Entrada interrumpida");
    return s.empty() ? defecto : s;
}

double pedir_numero(const string& etiqueta, const string& defecto) {
    string s = pedir_texto(etiqueta, defecto);
    size_t usados = 0;
    double v = stod(s, &usados);
    if (usados != s.size() || !isfinite(v)) throw runtime_error("Numero invalido: " + s);
    return v;
}

int main() {
    try {
        cout << "Proyecto 1 - Codificador de eco (C++17)\n";
        const string entrada = pedir_texto("Archivo WAV de entrada", "datos/vivaldi_10s.wav");
        Audio audio = leer_wav(entrada);
        const size_t n = audio.izquierdo.size();
        cout << "WAV correcto: " << audio.fs << " Hz, 2 canales, PCM 16 bits\n";
        cout << "Muestras por canal: " << n << "\n";
        cout << "Duracion: " << fixed << setprecision(5) << static_cast<double>(n) / audio.fs << " s\n";

        string canal = pedir_texto("Seleccione canal (I/D)", "I");
        if (canal != "I" && canal != "i" && canal != "D" && canal != "d")
            throw runtime_error("Canal invalido; use I o D");
        bool es_izquierdo = (canal == "I" || canal == "i");
        const vector<int16_t>& x = es_izquierdo ? audio.izquierdo : audio.derecho;
        cout << "Canal seleccionado: " << (es_izquierdo ? "Izquierdo" : "Derecho") << "\n";

        double tau0 = pedir_numero("Retardo bit 0 (microsegundos)", "1000");
        double tau1 = pedir_numero("Retardo bit 1 (microsegundos)", "2000");
        double pct0 = pedir_numero("Atenuacion bit 0 (%)", "20");
        double pct1 = pedir_numero("Atenuacion bit 1 (%)", "20");
        if (tau0 <= 0 || tau1 <= 0 || pct0 < 0 || pct0 >= 100 || pct1 < 0 || pct1 >= 100)
            throw runtime_error("Retardos deben ser positivos y atenuaciones estar entre 0 y menos de 100%");
        double d0_real = tau0 * audio.fs / 1000000.0;
        double d1_real = tau1 * audio.fs / 1000000.0;
        if (d0_real > 1e8 || d1_real > 1e8)
            throw runtime_error("Retardo demasiado grande");
        size_t D0 = static_cast<size_t>(llround(d0_real));
        size_t D1 = static_cast<size_t>(llround(d1_real));
        if (D0 == 0 || D1 == 0 || D0 == D1)
            throw runtime_error("Retardos deben corresponder a muestras positivas y distintas");
        const double a0 = pct0 / 100.0, a1 = pct1 / 100.0;
        const size_t maxD = max(D0, D1);
        const size_t ventana = static_cast<size_t>(llround(0.05 * audio.fs));
        if (maxD + 1 >= ventana)
            throw runtime_error("Retardo excesivo para ventana de 50 ms");
        const size_t ventanas_disponibles = n / ventana;
        if (ventanas_disponibles == 0) throw runtime_error("Audio demasiado corto");
        cout << "Retardos: D0=" << D0 << " muestras; D1=" << D1 << " muestras\n";
        cout << "Atenuaciones: a0=" << a0 << "; a1=" << a1 << "\n";
        cout << "Ventana: " << ventana << " muestras (50 ms)\n";
        cout << "Capacidad: " << ventanas_disponibles << " bits\n";

        string bits = pedir_texto("Mensaje binario", "101101");
        if (bits.empty() || bits.size() > ventanas_disponibles ||
            bits.find_first_not_of("01") != string::npos)
            throw runtime_error("Mensaje invalido: solo 0 y 1, maximo un bit por ventana");

        // Overlap-Add: primero copiar el audio original. Cada ventana aporta
        // un eco retardado a partir de sus propias muestras. Los ecos que
        // sobrepasan el limite de la ventana se suman a la siguiente.
        vector<double> salida(n + maxD, 0.0);
        for (size_t i = 0; i < n; ++i) salida[i] = static_cast<double>(x[i]);
        for (size_t b = 0; b < bits.size(); ++b) {
            const size_t D = bits[b] == '0' ? D0 : D1;
            const double a = bits[b] == '0' ? a0 : a1;
            const size_t inicio = b * ventana;
            for (size_t j = 0; j < ventana; ++j) {
                salida[inicio + j + D] += a * static_cast<double>(x[inicio + j]);
            }
        }

        const string destino = pedir_texto("Archivo WAV de salida", "datos/vivaldi_codificado_cpp.wav");
        if (destino == entrada) throw runtime_error("Salida no puede sobrescribir archivo de entrada");
        size_t recortes = 0;
        escribir_wav(destino, audio, salida, es_izquierdo, recortes);

        cout << "\nCODIFICACION FINALIZADA\n";
        cout << "Mensaje: " << bits << " (" << bits.size() << " bits)\n";
        cout << "Muestras de salida: " << salida.size() << "\n";
        cout << "Canal no seleccionado: sin modificaciones en las muestras originales\n";
        cout << "Muestras saturadas por clipping: " << recortes << "\n";
        cout << "Archivo generado: " << destino << "\n";
        return 0;
    } catch (const exception& e) {
        cerr << "ERROR: " << e.what() << '\n';
        return 1;
    }
}
