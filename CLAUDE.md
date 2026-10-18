# CLAUDE.md — librent

## Contexto

librent es un fork propio de LiberNT (sistema operativo libre de arquitectura NT,
derivado de ReactOS) que añade funciones que upstream no tiene; la primera es
ejecutar programas DOS en 64 bits con NTVDM. Cliente: proyecto personal de Hector.
Stack: C al estilo LiberNT, CMake + Ninja con RosBE (Clang por defecto y GCC),
Fast486, Win32, kernel32 + basesrv (CSR); pruebas con Python 3, QEMU/KVM, QMP y
mtools.

## Documentación del proyecto

La documentación funcional vive en `docs/`:

- `librent-maestro.md` — visión global, mapa de fases y estado del proyecto
- `librent-fase1-informe.md` — especificación funcional completa de la fase
- `librent-fase1-checklist.md` — alcance verificable de la fase
- `librent-fase1-implementacion.md` — informe de implementación (lo escribes tú)
- `usuario/` — documentación de usuario (la mantienes tú)

El informe de fase manda sobre tu criterio: si algo te parece incorrecto o
incompleto, no lo cambies por tu cuenta — impleméntalo como está o párate y
coméntalo con el desarrollador.

## Reglas de upstream (`AGENTS.md`)

Lee `AGENTS.md` (y el de cada subcarpeta que toques) antes de empezar. Sus reglas
valen en este fork salvo en estos puntos, que este archivo sustituye:

1. **Idioma.** Respuestas al desarrollador, documentos de `docs/` y `NOVEDADES.md`
   en castellano. Código, mensajes de commit y recursos base en inglés, como en
   upstream; los textos de interfaz nuevos llevan además su traducción `es-ES`.
2. **Pruebas en VM.** Las reglas que citan `vm_monitor.py` no aplican: esa
   herramienta no está publicada. Usa `scripts/librent/vmtest.py`.
3. **Paridad con Windows 11.** No aplica a NTVDM ni al soporte DOS y de 16 bits:
   Windows 11 no lo tiene. La referencia es la documentación de MS-DOS, el
   comportamiento de DOSBox y los ficheros de `scripts/librent/corpus/golden/`.
4. **Commits.** Ver la sección «Commits» de este archivo.

Lo demás de `AGENTS.md` se cumple tal cual: clean-room, prohibido el código
filtrado (NTVDMx64 incluido) y el código GPL de Linux, neutralidad de
arquitectura, ver cada arreglo funcionar en un arranque real y sin comentarios
en los diffs.

## Reglas de trabajo

- Escribe siempre en castellano tus respuestas y la documentación. Solo se
  mantienen en inglés los términos técnicos estándar (API, CRUD, endpoint,
  commit...) y lo que `AGENTS.md` pide en inglés (código y commits).
- No añadas funcionalidades que no estén en el checklist de la fase. Si detectas
  algo necesario que falta, propónlo antes de implementarlo y déjalo anotado
  como desviación.
- Nomenclatura: la de `CODING_STYLE.md`. Tipos y estructuras en
  `MAYUSCULAS_SNAKE_CASE`, funciones, variables y campos en `CamelCase` sin
  guiones bajos, opciones de menuconfig en `MAYUSCULAS_SNAKE_CASE` (`ENABLE_*`),
  valores de registro en `CamelCase`, claves de ficheros JSON y de texto en
  `minusculas_snake_case`.
- Zonas del fork: `subsystems/mvdm`, `modules/rostests/tests/ntvdm16`,
  `scripts/librent`, `docs`, `CLAUDE.md`, `LIBRENT_VERSION` y `NOVEDADES.md`.
  Fuera de ellas, toca solo los ficheros que liste el informe de la fase; cualquier
  otro cambio es una desviación que se anota.
- Sin `#ifdef` de arquitectura nuevos. Antes de dar algo por terminado, compila
  amd64, i386 y arm64.
- Trabaja en una rama por fase (`fase1-ntvdm`) y deja `main` para las fusiones.
- Cada vez que implementes funcionalidad nueva: (1) añade la documentación de
  usuario en `docs/usuario/`, (2) sube la versión en `LIBRENT_VERSION` y
  (3) escribe sus novedades en `NOVEDADES.md`, en lenguaje para usuarios.

### Comandos

```sh
. "$HOME/.local/opt/rosbe/rosbe-env.sh"          # entorno RosBE
export CCACHE_DISABLE=1                          # como pide AGENTS.md

./configure.sh -a amd64                          # Clang, Debug → output-Clang-amd64-debug
ninja -C output-Clang-amd64-debug liberntimg     # LiberNT.img
./configure.sh -a i386 --gcc                     # comprobación GCC i386
ninja -C output-GCC-i386-debug bootcd
./configure.sh -a arm64                          # comprobación arm64
ninja -C output-Clang-arm64-debug ntvdm command

# Pruebas (imagen con el arnés activado)
./configure.sh -a amd64 -DENABLE_NTVDM_BOOT_RUN=ON
ninja -C output-Clang-amd64-debug liberntimg
python3 scripts/librent/vmtest.py all \
    --image output-Clang-amd64-debug/LiberNT.img \
    --build-dir output-Clang-amd64-debug
```

El arnés necesita `qemu-system-x86_64` con KVM (usuario en el grupo `kvm`), `mtools`
y Python 3. Los binarios del corpus se descargan a `~/.cache/librent/corpus`, nunca
al repositorio.

## Sincronización con LiberNT

La fusión con upstream es semanal. La hace el desarrollador, o tú si te lo pide:

1. `git fetch upstream`
2. Rama `sync/AAAAMMDD` desde `main` y `git merge upstream/main-nt10`.
3. Conflictos: fuera de las zonas del fork manda upstream; en los ficheros de
   upstream que toca el fork, vuelve a aplicar el cambio mínimo.
4. Compila amd64, i386 y arm64 y pasa `vmtest.py all` en amd64.
5. PR a `main` del fork.

Nunca reescribas historia publicada. Las PR van siempre contra el fork
(`gh repo set-default` apunta a él); nunca abras una PR contra
`ahmedarif193/LiberNT`.

## Al terminar una fase (o al pausar la sesión)

1. Busca el archivo markdown que tenga `checklist` en el nombre correspondiente
   a esa fase y marca `[x]` todo lo que se haya realizado. Actualiza los
   contadores de progreso del final del archivo. No reescribas el texto de las
   funcionalidades: es la clave de trazabilidad con el resto de documentos.
2. Genera o actualiza `docs/librent-faseN-implementacion.md` con exactamente
   esta estructura:
   - Resumen de lo implementado
   - Desviaciones respecto a la especificación (qué y por qué)
   - Estructura de archivos creada/modificada
   - Decisiones técnicas tomadas durante el desarrollo
   - Funcionalidades del checklist completadas (copiando su texto exacto)
   - Pendientes y bloqueos
   - Ejecución y pruebas (cómo arrancar, migrar y testear)

   Es un documento vivo: actualízalo de forma incremental en cada sesión, nunca
   lo regeneres desde cero. Sé honesto en las desviaciones y en los pendientes;
   ese informe es lo único que el analista funcional verá de lo que realmente
   pasó, y con él se actualiza la especificación.
3. Recuérdale al desarrollador que debe entregar el informe de implementación al
   analista funcional antes de especificar la siguiente fase.
4. Invita al desarrollador a hacer PR. Si te dice que lo hagas, usa el comando
   `gh`, siempre contra el fork.

## Commits

- NUNCA firmes ni te atribuyas los commits: la autoría es del usuario.
- Sin `Co-Authored-By`, sin líneas de "Generated with", sin menciones a Claude
  en el mensaje.
- Formato de upstream (`.gitmessage`): título `[COMPONENTE] resumen` en inglés,
  en imperativo y sin punto final; cuerpo con `Problem:`, `Why:` y `How:`, o
  `feat:` para funciones cortas.
- Haz commit solo cuando el desarrollador lo pida, como dice `AGENTS.md`.
