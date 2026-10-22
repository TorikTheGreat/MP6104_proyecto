"""
Prototipo de ecoesteg en Python, completo de punta a punta. 
Codifica metadatos en un WAV o los recupera, con los mismos argumentos 
que tendrá el programa final. Se divide así para poder usar el programa
con reales reales de audio (aquí), y con pruebas usando test_aplicacion.py

    python prototipo/punta_a_punta.py --ayuda
"""

import os
import sys

import aplicacion


def main() -> int:
    args = [os.fsencode(a) for a in sys.argv[1:]]
    return aplicacion.ejecutar(args, sys.stdout, sys.stderr, aplicacion.bloques_reales())


if __name__ == "__main__":
    sys.exit(main())
