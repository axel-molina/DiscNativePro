---
name: release-docs-sync
description: Sincroniza documentación y web con los cambios de DiscNativePro al cerrar un release/bump de versión. Revisa y ACTUALIZA directamente docs/manual_usuario.md y README.md, y PROPONE cambios concretos para la web (DiscNativeProWeb) esperando aprobación. Se activa en cada release, bump de versión, tag nuevo o cierre de fase ("lo dejaremos hasta acá", "generar release", "nueva versión").
---

# Release Docs Sync

## Objetivo

Al cerrar un release de DiscNativePro, garantizar que el manual de usuario, el README y la web reflejen lo que la app hace ahora, y proponer al usuario qué cambios importantes mostrar públicamente.

## Paso 1 — Determinar el alcance del release

1. Leer la versión en `CMakeLists.txt` (`project(DiscNativePro VERSION x.y.z)`).
2. Ejecutar `git tag --sort=-creatordate | head -3` y `git log --oneline <último-tag>..HEAD` para listar commits del release (incluye trabajo de otros agentes — todo es trabajo legítimo, nada se descarta).
3. Leer las entradas nuevas de `walkthrough.md` para conocer features/fixes con detalle de UI (nombres de botones, secciones de Settings, flujos).

Construir una lista mental: **features nuevas**, **fixes visibles para el usuario**, **cambios de instalación/paquetes**.

## Paso 2 — Actualizar `docs/manual_usuario.md` (directo, sin pedir aprobación)

1. Leer el manual completo y comparar contra la lista del Paso 1.
2. Actualizar/insertar las secciones que falten o queden desactualizadas:
   - Nuevos controles y dónde viven (deck, Settings → pestaña exacta).
   - Nuevos flujos (ej: cómo separar stems, cómo importar mappings VDJ/Mixxx).
   - Cambios de instalación (DMG drag-to-Applications, MSIX, .deb).
   - Atajos MIDI nuevos si el manual documenta mappings.
3. Escribir en el mismo estilo/tone del manual existente, en español, sin placeholders ni TODOs.
4. No reescribir de más: solo las secciones afectadas por el release.

## Paso 3 — Actualizar `README.md` (directo, sin pedir aprobación)

1. Verificar: lista de features, badges, instrucciones de build, plataformas, versión mencionada si aplica.
2. Aplicar solo cambios necesarios para reflejar el release (features destacables y cambios de instalación). Mantener el idioma y estructura existentes.

## Paso 4 — Proponer cambios para la web (esperar aprobación)

La web vive en el repo hermano `DiscNativeProWeb` (Next.js 15 + Tailwind + framer-motion, export estático). Mapa de secciones en `src/app/page.tsx` (orden): Hero → SpecsTicker → StemsShowcase (`src/components/StemsShowcase.tsx`) → AiMatchShowcase → FeaturesGrid (grilla 3×N, `src/components/FeaturesGrid.tsx`) → ComparisonTable → DownloadSection (`src/components/DownloadSection.tsx`, usa `src/lib/detectOS.ts`) → MacInstructionsModal.

1. Analizar qué features del release merecen visibilidad pública y clasificar:
   - **Debe aparecer**: feature mayor orientada al usuario final (ej: stems, key shift).
   - **Merece mención**: mejora secundaria (ticker, card pequeña).
   - **No mostrar**: internals, fixes de compilación/CI.
2. Presentar la propuesta al usuario con AskUserQuestion o texto, indicando por cada cambio: componente exacto, qué agregar/modificar, y el copy sugerido.
3. **No editar la web sin aprobación explícita del usuario.**
4. Si el usuario aprueba, implementar respetando el design system (paleta obsidian/acid/cyanDeck/orangeDeck/purple, animaciones de `@/lib/animations`: staggerContainer/fadeUp/scaleIn/viewportOnce). En listas animadas usar datos fijos (arrays de alturas/valores), nunca Math.random (hydration mismatch).
5. Tras implementar, verificar visualmente (build + preview con `python3 -m http.server` sobre `out/` o `next dev`) y luego desplegar con `npm run deploy` (fuerza gh-pages). Confirmar el commit/deploy al usuario.
6. Verificar que DownloadSection y las instrucciones de instalación apunten al release nuevo (URL del tag) si la web lo referenciaba por versión.

## Paso 5 — Informe final

Resumir: qué se actualizó en manual y README (secciones), qué se propuso para la web y su estado (aprobado/desplegado o pendiente), y la versión del release. No crear archivos de informe; reportar en el chat.

## Reglas

- Si el release incluye trabajo de otros agentes, incluirlo también en manual/README/web (no filtrar por autor).
- No inventar features: todo lo documentado debe salir de commits/walkthrough/código real.
- Mantener cero placeholders y cero TODOs en todo archivo editado.
