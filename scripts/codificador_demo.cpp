#include "codificador.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
using namespace std;

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
        // K configurable: el tamano depende del mayor retardo (R33).
        // K=25 es provisional: mantiene una duracion cercana a 50 ms
        // para los retardos de prueba. Debe ajustarse con el decodificador.
        double k_numero = pedir_numero("Factor de ventana K (N=K*Dmax)", "25");
        if (k_numero < 2 || k_numero > 100000 || floor(k_numero) != k_numero)
            throw runtime_error("K debe ser un entero entre 2 y 100000");
        const size_t K = static_cast<size_t>(k_numero);
        if (maxD > numeric_limits<size_t>::max() / K)
            throw runtime_error("Tamano de ventana demasiado grande");
        const size_t ventana = K * maxD;
        if (ventana <= maxD || ventana > n)
            throw runtime_error("La ventana debe ser mayor que Dmax y no exceder el audio");
        const size_t ventanas_disponibles = n / ventana;
        if (ventanas_disponibles == 0) throw runtime_error("Audio demasiado corto");
        cout << "Retardos: D0=" << D0 << " muestras; D1=" << D1 << " muestras\n";
        cout << "Atenuaciones: a0=" << a0 << "; a1=" << a1 << "\n";
        cout << "Ventana: " << ventana << " muestras ("
             << (1000.0 * ventana / audio.fs) << " ms), K=" << K << "\n";
        cout << "Capacidad: " << ventanas_disponibles << " bits\n";

        string bits = pedir_texto("Mensaje binario", "101101");
        if (bits.empty() || bits.size() > ventanas_disponibles ||
            bits.find_first_not_of("01") != string::npos)
            throw runtime_error("Mensaje invalido: solo 0 y 1, maximo un bit por ventana");

        vector<double> salida = codificar_eco(x, bits, D0, D1, a0, a1, ventana);

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
