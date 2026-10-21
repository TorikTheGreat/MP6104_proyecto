# Archivos de prueba

Fragmentos de piezas de géneros distintos (A32), en formato de CD (PCM 16 bits,
44.1 kHz, estéreo, Little Endian, R25). Se versionan en esta carpeta. `make datos`
los regenera ejecutando `scripts/obtener_datos.sh`: descarga los originales en
`datos/originales/` (no versionada) y recorta y convierte cada fragmento.

| Archivo | Género | Obra y autor | Fuente (URL) | Licencia | Fragmento |
|---|---|---|---|---|---|
| `clasica_vivaldi.wav` | Clásica (cuerdas) | Antonio Vivaldi — *Las cuatro estaciones, Primavera (RV 269), I. Allegro*; interpretación de John Harrison (violín) y Wichita State University Chamber Players, 6 de febrero de 2000 | https://commons.wikimedia.org/wiki/File:Vivaldi_-_Four_Seasons_1_Spring_mvt_1_Allegro_-_John_Harrison_violin.oga | CC BY-SA 4.0 | 1:25–1:45 (20 s) |
| `vivaldi_10s.wav` | Clásica (cuerdas) | Antonio Vivaldi — *Las cuatro estaciones, Primavera (RV 269), I. Allegro*; interpretación de John Harrison (violín) y Wichita State University Chamber Players, 6 de febrero de 2000 | https://commons.wikimedia.org/wiki/File:Vivaldi_-_Four_Seasons_1_Spring_mvt_1_Allegro_-_John_Harrison_violin.oga | CC BY-SA 4.0 | 1:25–1:35 (10 s) |
| `ragtime_joplin.wav` | Ragtime (piano) | Scott Joplin — *The Entertainer* (compuesta en 1902); grabación de Gnissah (Wikimedia Commons), 2024 | https://commons.wikimedia.org/wiki/File:Scott_Joplin_-_The_Entertainer_(1902).ogg | Dominio público | 0:05–0:25 (20 s) |
| `jazz_park.wav` | Jazz | *Jazz at the Park*, dúo de flauta y saxofón tenor; grabación de Manwithmetalpig (Wikimedia Commons), 12 de agosto de 2025 | https://commons.wikimedia.org/wiki/File:Jazz_at_the_park.ogg | CC0 1.0 (Dominio público) | 0:05–0:25 (20 s) |
| `electronica_sintetica.wav` | Electrónica (sintetizador) | Síntesis aditiva polifónica con textura estocástica (`scripts/obtener_datos.sh`, semilla 404) | Generación propia reproducible | CC0 1.0 (Dominio público) | 0:00–0:20 (20 s) |

Los fragmentos de Vivaldi son obras derivadas de una grabación con licencia
[CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/): se recortaron y se
convirtieron a PCM de 16 bits, 44.1 kHz, estéreo, y se distribuyen con la misma
licencia. Los demás originales son de dominio público
([CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/) en el caso del jazz).
