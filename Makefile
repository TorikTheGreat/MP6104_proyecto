# Proyecto 1 - MP6104 Procesamiento Digital de Señales
# Esteganografía por enmascaramiento con eco
#
# Uso:  make | make test | make venv | make prototipo-test | make datos
#       make experimentos | make articulo | make pdf | make all | make clean
#       make distclean | make help
#
# Los objetivos funcionan aunque todavía no haya código: avisan y terminan
# sin error, para que la integración continua pase desde el primer commit.

SHELL    := /bin/bash

# ---- C++ ----------------------------------------------------------------
CXX      ?= g++
CXXFLAGS ?= -O2 -Wall -Wextra -Wpedantic
override CXXFLAGS += -std=c++17
CPPFLAGS += -Isrc -MMD -MP
LDLIBS   +=

BUILD    := build
BIN      := bin/ecoesteg
TEST_BIN := bin/pruebas

# Todo src/*.cpp menos main.cpp forma la biblioteca que comparten el
# programa y las pruebas; las pruebas son tests/*.cpp.
SRC      := $(filter-out src/main.cpp,$(wildcard src/*.cpp))
OBJ      := $(SRC:src/%.cpp=$(BUILD)/%.o)
TEST_SRC := $(wildcard tests/*.cpp)
TEST_OBJ := $(TEST_SRC:tests/%.cpp=$(BUILD)/tests/%.o)

# ---- Python (prototipo y experimentos) ----------------------------------
VENV     := .venv
# Otro intérprete con las dependencias: make prototipo-test PY=python3
PY       ?= $(VENV)/bin/python
PIP      := $(VENV)/bin/pip
USA_VENV := $(if $(filter $(VENV)/bin/python,$(PY)),venv)

# ---- Artículo -----------------------------------------------------------
TEXDIR   := articulo
PDF      := $(TEXDIR)/articulo.pdf

.PHONY: programa all test venv prototipo-test experimentos datos articulo pdf clean distclean help

ifneq ($(wildcard src/main.cpp),)
programa: $(BIN)
else
programa:
	@echo "Todavía no hay src/main.cpp: no hay programa que compilar."
endif

help:
	@echo "Objetivos disponibles:"
	@echo "  make                - compila bin/ecoesteg (implementación final en C++)"
	@echo "  make test           - compila y corre las pruebas de C++ (tests/*.cpp)"
	@echo "  make venv           - crea .venv e instala prototipo/requirements.txt"
	@echo "  make prototipo-test - corre las pruebas del prototipo (prototipo/tests/test_*.py)"
	@echo "  make datos          - regenera los fragmentos WAV de prueba (scripts/obtener_datos.sh)"
	@echo "  make experimentos   - corre los experimentos (prototipo/experimentos_*.py)"
	@echo "  make articulo       - compila el artículo en articulo/build/"
	@echo "  make pdf            - copia el PDF de entrega a articulo/articulo.pdf"
	@echo "  make all            - programa + pruebas + artículo"
	@echo "  make clean          - borra build/, bin/ y temporales de LaTeX"
	@echo "  make distclean      - clean + borra .venv"

all: programa test articulo

$(BIN): $(BUILD)/main.o $(OBJ)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDLIBS)

$(BUILD)/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(BUILD)/tests/%.o: tests/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(TEST_BIN): $(TEST_OBJ) $(OBJ)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDLIBS)

ifneq ($(TEST_SRC),)
test: $(TEST_BIN)
	./$(TEST_BIN)
else
test:
	@echo "Todavía no hay pruebas de C++ en tests/."
endif

-include $(OBJ:.o=.d) $(BUILD)/main.d $(TEST_OBJ:.o=.d)

# ---- Python -------------------------------------------------------------
# La marca .instalado solo se crea si pip termina bien, así que una
# instalación fallida se reintenta en la siguiente invocación.
$(VENV)/.instalado: prototipo/requirements.txt
	python3 -m venv $(VENV)
	$(PIP) install --upgrade pip
	$(PIP) install -r prototipo/requirements.txt
	@touch $@

venv: $(VENV)/.instalado

# prototipo/tests/ necesita un __init__.py para que unittest lo encuentre.
ifneq ($(wildcard prototipo/tests/test_*.py),)
prototipo-test: $(USA_VENV)
	$(PY) -m unittest discover -s prototipo/tests -t prototipo -v
else
prototipo-test:
	@echo "Todavía no hay pruebas del prototipo en prototipo/tests/."
endif

datos:
	@if [ -f scripts/obtener_datos.sh ]; then bash scripts/obtener_datos.sh; \
	else echo "Todavía no hay scripts/obtener_datos.sh."; fi

# Cada guion prototipo/experimentos_*.py recibe --datos, --resultados y --programa.
experimentos: $(USA_VENV) programa
	@mkdir -p figuras resultados
	@set -e; hay=0; for e in prototipo/experimentos_*.py; do \
	  [ -f "$$e" ] || continue; hay=1; \
	  $(PY) "$$e" --datos datos --resultados resultados --programa $(BIN); done; \
	[ $$hay = 1 ] || echo "Todavía no hay experimentos en prototipo/."

# ---- Artículo -----------------------------------------------------------
# latexmk compila en articulo/build/ (ignorado por git). 'make pdf' además
# copia el PDF a articulo/articulo.pdf, que SÍ se versiona porque es parte de
# la entrega; lo corre solo el responsable de la entrega.
articulo:
	cd $(TEXDIR) && latexmk -pdf -outdir=build articulo.tex
	@echo "PDF de trabajo: $(TEXDIR)/build/articulo.pdf"

pdf: articulo
	@cp $(TEXDIR)/build/articulo.pdf $(PDF)
	@echo "PDF de entrega: $(PDF)"

# ---- Limpieza -----------------------------------------------------------
clean:
	@rm -rf $(BUILD) bin
	cd $(TEXDIR) && latexmk -C -outdir=build >/dev/null 2>&1 || true
	@rm -rf $(TEXDIR)/build
	@echo "Objetos, binarios y temporales de LaTeX eliminados."

distclean: clean
	@rm -rf $(VENV)
	@echo "Entorno virtual eliminado."
