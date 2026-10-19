/*
Línea de comandos de ecoesteg (sección 5.2 del enunciado; equivale a
prototipo/linea_comandos.py).

analizar() valida los argumentos y arma la configuración; no abre archivos ni
ejecuta nada. Los argumentos son bytes tal cual (std::string), sin suponer que
sean UTF-8 válido. Los números se validan con su forma antes de convertirlos,
para aceptar exactamente lo mismo que Python; tests/pruebas_linea_comandos.cpp
compara con los casos de prototipo/vectores_linea_comandos.py. Los retardos y
atenuaciones no dados quedan vacíos y resolver() les pone los valores por defecto.
*/

#ifndef LINEA_COMANDOS_HPP
#define LINEA_COMANDOS_HPP

#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "trama.hpp"

namespace linea_comandos {

constexpr int EXITO = 0;
constexpr int ERROR_EJECUCION = 1;  // archivo, formato WAV; mensaje que no cabe con SiNoCabe::error
constexpr int ERROR_USO = 2;        // R7 (parámetros desconocidos) y valores inválidos

// Texto de --ayuda, con los valores por defecto de parametros.hpp.
const std::string& uso();

enum class Modo { codificar, decodificar, ayuda };

const char* nombre_modo(Modo modo);  // "codificar", "decodificar" o "ayuda"

struct Configuracion {
    Modo modo = Modo::decodificar;
    std::string entrada;
    std::string salida;            // vacía al decodificar
    trama::Metadatos metadatos;    // categoría -> bytes UTF-8
    std::optional<std::int64_t> retardo0_us;
    std::optional<std::int64_t> retardo1_us;
    std::optional<double> atenuacion0;  // %
    std::optional<double> atenuacion1;
    std::string canal = "izquierdo";
    std::optional<std::string> diagnostico;
};

struct ErrorUso {
    std::string mensaje;
};

// args: los argumentos sin el nombre del programa.
std::variant<Configuracion, ErrorUso> analizar(const std::vector<std::string>& args);

struct Enmascaramiento {
    std::int64_t retardo0_us;
    std::int64_t retardo1_us;
    double atenuacion0;  // %
    double atenuacion1;
};

// Los parámetros de enmascaramiento, con los por defecto en lo que no se dio (R17).
Enmascaramiento resolver(const Configuracion& c);

}  // namespace linea_comandos

#endif
