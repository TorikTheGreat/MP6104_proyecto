"""
Interfaz de un formato de trama.

Un formato es un módulo con dos funciones:
    codificar(metadatos) -> bits de una copia del mensaje
        metadatos: {categoría: texto (str o bytes UTF-8)}
    leer(bits) -> lista de (tipo, categoría, datos), en el orden en que se leen,
        con repetidos; tipo es VALIDO, DESCONOCIDO o DANADO.

metadatos.py elige el formato con parametros.FORMATO_TRAMA; la repetición, la
selección de lo que se reporta y la notificación no dependen del formato.
"""

Bits = list[int]                    # un bit (0 o 1) por ventana
Metadatos = dict[int, str | bytes]  # categoría -> texto
Lectura = tuple[str, int, bytes]    # (tipo, categoría, datos)

VALIDO = "valido"
DESCONOCIDO = "desconocido"  # se leyó bien, pero la categoría o el texto no son válidos
DANADO = "danado"            # la comprobación de errores del formato falló
