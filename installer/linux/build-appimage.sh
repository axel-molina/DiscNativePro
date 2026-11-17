#!/usr/bin/env bash
# P33: empaqueta DiscNativePro como AppImage (AppDir + linuxdeploy + plugin gtk).
#
# Uso: ejecutar desde la raíz del repo con build/ ya compilado en Release.
#   cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build
#   installer/linux/build-appimage.sh
#
# Alcance (A): el AppImage es autocontenido para audio + GUI. El runtime ONNX
# va junto al binario (rpath $ORIGIN) y las librerías del sistema se empaquetan
# con linuxdeploy. El video de YouTube depende de WebKitGTK: se empaqueta lo que
# linuxdeploy detecte, pero sus procesos auxiliares (WebKitWebProcess,
# WebKitNetworkProcess) y los plugins de GStreamer NO se empaquetan, por lo que
# el video puede no funcionar dentro del AppImage.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

APP_NAME="DiscNativePro"
APPDIR="$ROOT/AppDir"
OUTPUT="$ROOT/${APP_NAME}-Linux-x86_64.AppImage"
LINUXDEPLOY="linuxdeploy-x86_64.AppImage"
# El plugin gtk dejó de publicarse como AppImage: el release "continuous"
# ya no existe (404). La fuente oficial es este script .sh del repositorio.
GTK_PLUGIN="linuxdeploy-plugin-gtk.sh"

echo "==> Armando AppDir"
rm -rf "$APPDIR"
# Se borra el AppImage previo: el find final usa un patrón que también lo matchea.
rm -f "$OUTPUT"
mkdir -p "$APPDIR/usr/bin" \
         "$APPDIR/usr/share/applications" \
         "$APPDIR/usr/share/icons/hicolor/256x256/apps"

EXE="$(find build -type f -name "$APP_NAME" -perm /111 | head -1)"
if [ -z "$EXE" ]; then
  echo "ERROR: no se encontró el binario '$APP_NAME' en build/." >&2
  echo "       Compilá primero: cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build" >&2
  exit 1
fi
cp "$EXE" "$APPDIR/usr/bin/$APP_NAME"
chmod +x "$APPDIR/usr/bin/$APP_NAME"

# Runtime ONNX: el loader resuelve el SONAME 'libonnxruntime.so.1', no el nombre
# versionado del archivo. Se copia el .so real y se crea el enlace con ese
# nombre; sin él la app no arranca (DT_NEEDED no resoluble).
ORT_REAL="$(find build -type f -name 'libonnxruntime.so.*.*' | head -1)"
if [ -n "$ORT_REAL" ]; then
  cp "$ORT_REAL" "$APPDIR/usr/bin/"
  ln -sf "$(basename "$ORT_REAL")" "$APPDIR/usr/bin/libonnxruntime.so.1"
  echo "    ONNX runtime: $(basename "$ORT_REAL") (+ enlace libonnxruntime.so.1)"
else
  echo "WARNING: no se encontró libonnxruntime.so.* en build/; la app no arrancará" >&2
  echo "         (DT_NEEDED 'libonnxruntime.so.1' sin resolver) y los stems degradarán." >&2
  exit 1
fi

ORT_PROVIDERS="$(find build -type f -name 'libonnxruntime_providers_shared.so' | head -1)"
if [ -n "$ORT_PROVIDERS" ]; then
  cp "$ORT_PROVIDERS" "$APPDIR/usr/bin/"
fi

cp install/linux/discnativepro.desktop "$APPDIR/usr/share/applications/discnativepro.desktop"
cp assets/icon.png "$APPDIR/usr/share/icons/hicolor/256x256/apps/discnativepro.png"

echo "==> Descargando linuxdeploy y el plugin gtk"
curl -fsSL -o "$LINUXDEPLOY" \
  "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/${LINUXDEPLOY}"
curl -fsSL -o "$GTK_PLUGIN" \
  "https://raw.githubusercontent.com/linuxdeploy/linuxdeploy-plugin-gtk/master/linuxdeploy-plugin-gtk.sh"
chmod +x "$LINUXDEPLOY" "$GTK_PLUGIN"

# Los runners de GitHub no tienen FUSE: el AppImage se ejecuta desempaquetado.
export APPIMAGE_EXTRACT_AND_RUN=1
export ARCH=x86_64

echo "==> Generando AppImage"
"./${LINUXDEPLOY}" \
  --appdir "$APPDIR" \
  --plugin gtk \
  --output appimage \
  --desktop-file install/linux/discnativepro.desktop \
  --icon-file assets/icon.png

GENERATED="$(find . -maxdepth 1 -type f -name "${APP_NAME}*-x86_64.AppImage" | head -1)"
if [ -z "$GENERATED" ]; then
  echo "ERROR: linuxdeploy no generó ningún AppImage." >&2
  exit 1
fi
mv "$GENERATED" "$OUTPUT"

# El AppImage es autocontenido: se limpian los artefactos intermedios.
rm -rf "$APPDIR" "$LINUXDEPLOY" "$GTK_PLUGIN"

echo "==> AppImage listo: $OUTPUT ($(du -h "$OUTPUT" | cut -f1))"
echo "    Recordatorio: los modelos de stems no se empaquetan (gitignored); sin ellos"
echo "    la separación degrada al modo completo. yt-dlp/ffmpeg se descargan en runtime."
