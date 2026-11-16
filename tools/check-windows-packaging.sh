#!/usr/bin/env bash
# Guarda de regresión del empaquetado de Windows (Notion #14).
#
# La app de Windows carga ONNX Runtime de forma ESTÁTICA al arrancar (import de
# onnxruntime.lib), así que el loader de Windows necesita estas dos DLL junto al
# .exe. La v1.4.0 se publicó sin ellas en los 3 artefactos (ZIP del CI,
# instalador Inno Setup y paquete MSIX) y la app no arrancaba (0xc000007b).
#
# Falla si: las DLL vendorizadas no están en el repo, o algún camino de
# empaquetado no las copia.
set -euo pipefail

cd "$(dirname "$0")/.."

ORT_DLLS=("onnxruntime.dll" "onnxruntime_providers_shared.dll")
VENDOR_DIR="third_party/onnxruntime/windows/lib"
PACKAGING_FILES=(
  ".github/workflows/build-windows.yml"   # ZIP del CI
  ".github/workflows/release.yml"          # instalador Inno Setup + MSIX
  "installer/windows/installer.iss"       # contenido del instalador
)

fail=0

for dll in "${ORT_DLLS[@]}"; do
  if [[ ! -f "$VENDOR_DIR/$dll" ]]; then
    echo "FALTA la DLL vendorizada: $VENDOR_DIR/$dll"
    fail=1
  fi
done

for f in "${PACKAGING_FILES[@]}"; do
  if [[ ! -f "$f" ]]; then
    echo "FALTA el archivo de empaquetado: $f"
    fail=1
    continue
  fi
  for dll in "${ORT_DLLS[@]}"; do
    if ! grep -q "$dll" "$f"; then
      echo "FALTA $dll en $f (el paquete de Windows se publicaría sin ella)"
      fail=1
    fi
  done
done

if [[ $fail -ne 0 ]]; then
  echo "Empaquetado de Windows incompleto: la app no arrancaría en Windows."
  exit 1
fi

echo "OK: ONNX Runtime vendorizado y presente en los 3 caminos de empaquetado de Windows."
