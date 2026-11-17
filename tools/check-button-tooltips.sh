#!/usr/bin/env bash
# Guarda de regresión de los tooltips (Notion #10).
#
# El ticket #10 cerró el hueco de que 88 botones de la app no tuvieran
# tooltip (los que ya tenían, concentrados en ~39 llamadas). Este check
# recorre los headers de src/ui, saca los miembros de tipo boton y los cruza
# con todos los setTooltip del proyecto: si alguien agrega un boton nuevo y
# olvida el tooltip, el check falla.
#
# Uso: bash tools/check-button-tooltips.sh
set -euo pipefail

cd "$(dirname "$0")/.."

BUTTON_TYPES='DjButton|juce::TextButton|juce::ImageButton|juce::ToggleButton'

buttons=$(grep -rhoE "(${BUTTON_TYPES})[[:space:]]+[a-zA-Z_][a-zA-Z0-9_]*[[:space:]]*;" \
            src --include=*.h | sed -E 's/.*[[:space:]]([a-zA-Z_][a-zA-Z0-9_]*)[[:space:]]*;/\1/' | sort -u)

tooltips=$(grep -rhoE "[a-zA-Z_][a-zA-Z0-9_]*\.setTooltip" \
             src --include=*.cpp --include=*.h | sed 's/\.setTooltip$//' | sort -u)

total=$(echo "$buttons" | grep -c . || true)
missing=0

for b in $buttons; do
  if ! echo "$tooltips" | grep -qx "$b"; then
    echo "SIN TOOLTIP: $b"
    missing=$((missing + 1))
  fi
done

if [ "$missing" -ne 0 ]; then
  echo ""
  echo "$missing de $total botones miembro de src/ui no tienen setTooltip."
  exit 1
fi

echo "OK: los $total botones miembro de src/ui tienen tooltip."
