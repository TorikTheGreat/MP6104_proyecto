/*Definición de una serie de parámetros usados a lo largo del proyecto. 
Usa nombres y reglas equivalentes a las de prototipo/parametros.py.

Las conversiones usan aritmética entera o la misma operación en coma flotante,
para que Python y C++ den los mismos números (prototipo/vectores_parametros.py).
Los valores por defecto (R18) y K_VENTANA (R33, R42) son provisionales. Los valores
finales serán definidos experimentalmente más adelante.
Solo encabezado: todo es constexpr o inline.
*/

#ifndef PARAMETROS_HPP
#define PARAMETROS_HPP

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace parametros {

constexpr int FS = 44100;  // Hz (R25)

// Categorías de metadatos (R10-R16).
constexpr int TITULO = 1;
constexpr int ARTISTA = 2;
constexpr int AUTOR = 3;
constexpr int ALBUM = 4;
constexpr int ANIO = 5;
constexpr int INVITADO = 6;
constexpr int PRODUCTOR = 7;

// Nombre para notificar (R49); nullptr si la categoría no es 1-7.
inline const char* nombre_categoria(int categoria) {
    switch (categoria) {
        case TITULO: return "Título";
        case ARTISTA: return "Artista";
        case AUTOR: return "Autor";
        case ALBUM: return "Álbum";
        case ANIO: return "Año";
        case INVITADO: return "Artista invitado";
        case PRODUCTOR: return "Productor";
        default: return nullptr;
    }
}

// Retardos, en microsegundos (R19-R21).
constexpr std::int64_t RETARDO_MIN_US = 1;
constexpr std::int64_t RETARDO_MAX_US = 1000000;
constexpr std::int64_t RETARDO_MIN_UTIL_US = 12;  // el menor que da al menos 1 muestra

// Retardo en us -> muestras; la media muestra redondea hacia arriba. Con enteros
// porque std::round y round() de Python redondean distinto. Solo acepta enteros.
template <typename T, typename = std::enable_if_t<std::is_integral_v<T> &&
                                                  !std::is_same_v<T, bool>>>
constexpr std::int64_t muestras_desde_us(T t) {
    bool fuera = false;
    if constexpr (std::is_signed_v<T>)
        fuera = static_cast<long long>(t) < RETARDO_MIN_US ||
                static_cast<long long>(t) > RETARDO_MAX_US;
    else
        fuera = static_cast<unsigned long long>(t) <
                    static_cast<unsigned long long>(RETARDO_MIN_US) ||
                static_cast<unsigned long long>(t) >
                    static_cast<unsigned long long>(RETARDO_MAX_US);
    if (fuera)
        throw std::out_of_range("retardo fuera de [" + std::to_string(RETARDO_MIN_US) + ", " +
                                std::to_string(RETARDO_MAX_US) + "] us: " + std::to_string(t));
    return (static_cast<std::int64_t>(t) * FS + 500000) / 1000000;
}

//Estas siguientes son decisiones de diseño. Algunas vienen de ambiguedades en
//el enunciado.

// R22 Esta viene de la incertidumbre sobre lo que quiere decir atenuación aquí: 
// True -> la atenuación es la amplitud del eco (30 % -> a = 0.3);
// False -> el eco se atenúa ese porcentaje (30 % -> a = 0.7).
constexpr bool ATENUACION_ES_AMPLITUD = true;

// R31 Que hacer si no cabe ni una copia del mensaje: "truncar" (codifica lo que cabe y
// avisa) o simplemente devolver un error.
enum class SiNoCabe { truncar, error };
constexpr SiNoCabe SI_NO_CABE = SiNoCabe::truncar;

// True: el mensaje se repite hasta llenar las ventanas; False: va una vez y las
// demás ventanas quedan sin eco.
constexpr bool REPETIR_MENSAJE = true;

// Formato de la trama.
enum class FormatoTrama { registros };
constexpr FormatoTrama FORMATO_TRAMA = FormatoTrama::registros;

// R50: true -> desconocidos y dañados solo se avisan si hubo algún registro
// válido (un archivo sin codificar no da avisos); false -> se avisan siempre.
constexpr bool AVISOS_REQUIEREN_VALIDO = true;

// O2: true -> al decodificar se vota bit a bit entre las copias repetidas del
// mensaje (votacion.hpp) y la copia votada se lee como una copia más.
constexpr bool VOTAR_COPIAS = true;

// Atenuación en % (R22-R24) -> amplitud a_b del eco en H_b(z) = 1 + a_b z^(-t_b).
// El rango (0, 100) es el que acepta el codificador.
inline double amplitud_desde_porcentaje(double porcentaje) {
    if (!(porcentaje > 0.0 && porcentaje < 100.0))
        throw std::out_of_range("atenuación fuera de (0, 100) %");
    return ATENUACION_ES_AMPLITUD ? porcentaje / 100.0 : 1.0 - porcentaje / 100.0;
}

// int16 -> [-1, 1). Solo acepta std::int16_t, para que un int fuera de rango no
// se trunque sin aviso.
template <typename T, typename = std::enable_if_t<std::is_same_v<T, std::int16_t>>>
constexpr double a_flotante(T m) {
    return m / 32768.0;
}

// [-1, 1] -> int16: y*32768 saturado a [-32768, 32767] (el eco suma amplitud) y
// truncado hacia cero, como el escritor de WAV del codificador. NaN o infinito
// lanza std::domain_error (convertirlos a entero es comportamiento indefinido).
inline std::int16_t a_int16(double y) {
    if (!std::isfinite(y)) throw std::domain_error("a_int16: muestra no finita");
    return static_cast<std::int16_t>(std::clamp(y * 32768.0, -32768.0, 32767.0));
}

// Valores por defecto (R17, R18), elegidos con prototipo/ajuste_parametros.py.
constexpr std::int64_t RETARDO0_US = 750;   // 33 muestras
constexpr std::int64_t RETARDO1_US = 1500;  // 66 muestras
constexpr double ATENUACION0 = 30.0;
constexpr double ATENUACION1 = 30.0;

// Ventana = K veces el mayor retardo (R33, R42), elegido con
// prototipo/ajuste_ventana.py. ECOESTEG_K_VENTANA solo lo define ese experimento,
// que compila el programa con otros K.
#ifdef ECOESTEG_K_VENTANA
constexpr std::int64_t K_VENTANA = ECOESTEG_K_VENTANA;
#else
constexpr std::int64_t K_VENTANA = 25;
#endif

// Largo de la ventana en muestras.
constexpr std::int64_t largo_ventana(std::int64_t t0_us, std::int64_t t1_us) {
    return K_VENTANA * std::max(muestras_desde_us(t0_us), muestras_desde_us(t1_us));
}

// Bits distintos que se aceptan en un preámbulo esperado (trama).
constexpr int TOLERANCIA_PREAMBULO = 1;

}  

#endif  
