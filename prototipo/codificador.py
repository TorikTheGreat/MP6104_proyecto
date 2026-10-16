import numpy as np
from scipy.io import wavfile


# --------------------------------------------------
# 1. Función para generar la respuesta al impulso
# --------------------------------------------------

def generar_eco(D, a):
    h = np.zeros(D + 1)
    h[0] = 1
    h[D] = a
    return h


# --------------------------------------------------
# 2. Leer el archivo WAV
# --------------------------------------------------

Fs, audio = wavfile.read("datos/vivaldi_10s.wav")

print("Frecuencia de muestreo:", Fs)
print("Dimensiones del audio:", audio.shape)
print("Tipo de dato:", audio.dtype)


# --------------------------------------------------
# 3. Separar los canales izquierdo y derecho
# --------------------------------------------------

izquierdo = audio[:, 0]
derecho = audio[:, 1]

print("Muestras canal izquierdo:", len(izquierdo))
print("Muestras canal derecho:", len(derecho))

# Convertir a float64 para realizar las operaciones
izquierdo_float = izquierdo.astype(np.float64)

# Seleccionar el canal que se desea codificar
canal = input("Seleccione el canal (I/D) [I]: ").strip().upper()

if canal == "":
    canal = "I"

if canal not in ("I", "D"):
    raise ValueError("Seleccione I para izquierdo o D para derecho")

if canal == "I":
    canal_original = izquierdo.astype(np.float64)
else:
    canal_original = derecho.astype(np.float64)

print("Canal seleccionado:", canal)


# --------------------------------------------------
# 4. Definir los parámetros de los ecos
# --------------------------------------------------

# Retardos en microsegundos
retardo0_us = float(input("Retardo para bit 0 (us): "))
retardo1_us = float(input("Retardo para bit 1 (us): "))

# Atenuaciones en porcentaje
atenuacion0 = float(input("Atenuación para bit 0 (%): "))
atenuacion1 = float(input("Atenuación para bit 1 (%): "))

# Convertir porcentajes a coeficientes
a0 = atenuacion0 / 100
a1 = atenuacion1 / 100

# Validar parámetros
if retardo0_us <= 0 or retardo1_us <= 0:
    raise ValueError("Los retardos deben ser mayores que cero")

if not (0 <= a0 < 1 and 0 <= a1 < 1):
    raise ValueError("Las atenuaciones deben estar entre 0 y menos de 100 %")

# Convertir los retardos a muestras
D0 = round(retardo0_us * Fs / 1_000_000)
D1 = round(retardo1_us * Fs / 1_000_000)

if D0 <= 0 or D1 <= 0 or D0 == D1:
    raise ValueError("Los retardos deben producir muestras positivas y diferentes")

# Generar las respuestas al impulso
h0 = generar_eco(D0, a0)
h1 = generar_eco(D1, a1)



print("Retardo bit 0:", D0, "muestras")
print("Retardo bit 1:", D1, "muestras")
print("Longitud de h0:", len(h0))
print("Longitud de h1:", len(h1))


# --------------------------------------------------
# 5. Definir la duración de cada ventana
# --------------------------------------------------

duracion_ventana = 0.05

N_ventana = round(duracion_ventana * Fs)

if max(D0, D1) + 1 >= N_ventana:
    raise ValueError("Los retardos deben ser menores que la ventana")

cantidad_ventanas = len(canal_original) // N_ventana

print("Muestras por ventana:", N_ventana)
print("Ventanas completas:", cantidad_ventanas)


# --------------------------------------------------
# 6. Definir y validar el mensaje binario
# --------------------------------------------------

mensaje = input("Ingrese la secuencia de bits (0 y 1): ")

if not mensaje or any(bit not in "01" for bit in mensaje):
    raise ValueError("El mensaje debe contener únicamente 0 y 1")

bits = [int(bit) for bit in mensaje]

if len(bits) > cantidad_ventanas:
    raise ValueError("El mensaje supera la capacidad del audio")

print("Mensaje ingresado:", mensaje)
print("Cantidad de bits:", len(bits))


# --------------------------------------------------
# 7. Crear el arreglo de salida
# --------------------------------------------------

# Se reserva espacio para el audio y la cola del eco
salida = np.zeros(len(canal_original) + max(D0, D1))


# --------------------------------------------------
# 8. Codificar cada bit mediante convolución
#    y aplicar Overlap-Add
# --------------------------------------------------

for i in range(len(bits)):

    # Calcular los límites de la ventana
    inicio = i * N_ventana
    fin = inicio + N_ventana

    # Extraer las muestras de la ventana
    ventana = canal_original[inicio:fin]

    # Seleccionar el eco según el bit
    if bits[i] == 0:
        h = h0
    else:
        h = h1

    # Aplicar la convolución
    ventana_codificada = np.convolve(ventana, h, mode="full")

    # Sumar la ventana codificada a la señal de salida
    salida[inicio:inicio + len(ventana_codificada)] += ventana_codificada

    # Mostrar información de la ventana
    print("Ventana:", i, "Bit:", bits[i])
    print("Inicio:", inicio, "Fin:", fin)
    print("Longitud del eco:", len(h))
    print("Longitud codificada:", len(ventana_codificada))


# --------------------------------------------------
# 9. Conservar el resto del audio original
# --------------------------------------------------

# Calcular dónde termina el mensaje
fin_mensaje = len(bits) * N_ventana

# Copiar el audio que no contiene bits ocultos
salida[fin_mensaje:len(canal_original)] += canal_original[fin_mensaje:]


# --------------------------------------------------
# 10. Comprobar el resultado
# --------------------------------------------------

print("Longitud del audio original:", len(canal_original))
print("Longitud de salida:", len(salida))
print("Muestras distintas de cero:", np.count_nonzero(salida))

# --------------------------------------------------
# 11. Preparar el audio estéreo
# --------------------------------------------------

# Crear un arreglo estéreo con la longitud de salida
audio_estereo = np.zeros((len(salida), 2))

# Copiar los dos canales originales
audio_estereo[:len(audio), :] = audio

# Sustituir únicamente el canal seleccionado
if canal == "I":
    audio_estereo[:, 0] = salida
else:
    audio_estereo[:, 1] = salida
# --------------------------------------------------
# 12. Convertir a PCM de 16 bits
# --------------------------------------------------

# Evitar valores fuera del rango de int16
audio_estereo = np.clip(audio_estereo, -32768, 32767)

# Convertir a enteros de 16 bits
audio_estereo = audio_estereo.astype(np.int16)

# --------------------------------------------------
# 13. Guardar el archivo WAV
# --------------------------------------------------

wavfile.write("datos/vivaldi_codificado.wav", Fs, audio_estereo)

print("Archivo WAV codificado generado correctamente")
print("Dimensiones del audio codificado:", audio_estereo.shape)
print("Tipo de dato:", audio_estereo.dtype)

print("Amplitud máxima antes de convertir:", np.max(salida))
print("Amplitud mínima antes de convertir:", np.min(salida))

print("Muestras por encima de 32767:", np.count_nonzero(salida > 32767))
print("Muestras por debajo de -32768:", np.count_nonzero(salida < -32768))

# Comparar el canal original con el codificado
diferencia = salida[:len(canal_original)] - canal_original

print("Muestras modificadas:", np.count_nonzero(diferencia))
print("Diferencia máxima absoluta:", np.max(np.abs(diferencia)))

# Comprobar matemáticamente la ecuación del eco
n = max(D0, D1) + 1

valor_original = canal_original[n]

# La primera ventana utiliza el primer bit del mensaje
if bits[0] == 0:
    D = D0
    a = a0
else:
    D = D1
    a = a1

valor_retardado = canal_original[n - D]

valor_esperado = valor_original + a * valor_retardado
valor_obtenido = salida[n]

print("Muestra original:", valor_original)
print("Muestra retardada:", valor_retardado)
print("Valor esperado:", valor_esperado)
print("Valor obtenido:", valor_obtenido)
print("Error:", abs(valor_esperado - valor_obtenido))

# --------------------------------------------------
# 14. Verificar el canal no seleccionado
# --------------------------------------------------

# Leer nuevamente el archivo codificado
Fs_salida, audio_codificado = wavfile.read(
    "datos/vivaldi_codificado.wav"
)

if canal == "I":
    indice_no_modificado = 1
else:
    indice_no_modificado = 0

# Comparar únicamente las muestras originales
canal_sin_eco = audio_codificado[:len(audio), indice_no_modificado]
canal_referencia = audio[:, indice_no_modificado]

son_iguales = np.array_equal(canal_sin_eco, canal_referencia)

print("Canal no seleccionado idéntico al original:", son_iguales)

# --------------------------------------------------
# 15. Verificar todas las ventanas codificadas
# --------------------------------------------------

print("\nVerificación de las ventanas codificadas")

errores = 0

for i in range(len(bits)):

    inicio = i * N_ventana
    fin = inicio + N_ventana

    # Seleccionar los parámetros según el bit
    if bits[i] == 0:
        D = D0
        a = a0
    else:
        D = D1
        a = a1

    # Calcular el eco esperado para esta ventana
    ventana = canal_original[inicio:fin]

    eco_esperado = np.zeros(len(ventana) + D)
    eco_esperado[:len(ventana)] = ventana
    eco_esperado[D:] += a * ventana

    # Comparar con la contribución calculada por convolución
    h = generar_eco(D, a)
    eco_obtenido = np.convolve(ventana, h, mode="full")

    error = np.max(np.abs(eco_esperado - eco_obtenido))

    print("Ventana:", i,
          "Bit:", bits[i],
          "Error máximo:", error)

    if not np.allclose(eco_esperado, eco_obtenido, atol=1e-9):
        errores += 1

if errores == 0:
    print("PRUEBA SUPERADA: todas las ventanas son correctas")
else:
    print("PRUEBA FALLIDA:", errores, "ventanas incorrectas")

    # --------------------------------------------------
# 16. Verificar el resultado completo de Overlap-Add
# --------------------------------------------------

salida_esperada = np.zeros(len(salida))

# Primero conservar toda la señal original
salida_esperada[:len(canal_original)] = canal_original

# Agregar únicamente los ecos de cada ventana
for i in range(len(bits)):

    inicio = i * N_ventana
    fin = inicio + N_ventana

    if bits[i] == 0:
        D = D0
        a = a0
    else:
        D = D1
        a = a1

    ventana = canal_original[inicio:fin]

    # El eco comienza D muestras después
    salida_esperada[inicio + D:fin + D] += a * ventana

# Comparar ambas señales completas
error_maximo = np.max(np.abs(salida - salida_esperada))

print("\nVerificación de Overlap-Add")
print("Error máximo:", error_maximo)

if np.allclose(salida, salida_esperada, atol=1e-9):
    print("PRUEBA SUPERADA: Overlap-Add correcto")
else:
    print("PRUEBA FALLIDA: revisar Overlap-Add")