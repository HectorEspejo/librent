# librent - Fase 1: NTVDM en 64 bits (DOS en modo texto)

## Informe de implementación

**Última actualización:** 18/10/2026 (sesión 1: S0 y S1)

---

## Resumen de lo implementado

### S0 · Entorno

- RosBE local instalado en `~/.local/opt/rosbe` con el bootstrap oficial:
  LLVM 24 (nativo), LLVM-MinGW (ucrt, amd64/i386/arm64), LLVM-MinGW RISC-V y
  MinGW-GCC 16.2 (i686, x86_64, aarch64). Se verificó `rosbe-env.sh` y el
  `clang` de RosBE (24.0.0git).
- `mtools` ya estaba instalado (4.0.49); QEMU y KVM accesibles sin cambios.
- El Python de `mise` no traía `mako`, `packaging` ni `pyyaml`, que Mesa
  necesita al configurar; se instalaron con `pip --user`.
- Se configuraron builds para amd64 (Clang y GCC), i386 (GCC), arm64 (Clang),
  riscv64 y ppc (solo configuración).

### S1 · Fork y compilación

- Rama de trabajo renombrada de `fase1-dos` a `fase1-ntvdm`; `AGENTS.md`
  restaurado al de upstream (las reglas del fork viven en `CLAUDE.md`).
- Rama por defecto del fork cambiada a `main` (estaba en `main-nt10`).
- Primera sincronización con upstream: `git fetch upstream` y merge
  fast-forward de `upstream/main-nt10` en `main` y `fase1-ntvdm`.
  **Fecha: 18/10/2026. Commit de upstream: `97b438262df`
  («[NTOS:PO][POWRPROF] Apply the power button action of the active power
  scheme»), 41 commits por delante del punto de partida `20b4bc91cbe`.** El
  merge no produjo conflictos; al ser fast-forward no hubo PR (pendiente de
  decisión del desarrollador).
- `subsystems/mvdm` restaurado desde `054ce27^`: 208 ficheros (ntvdm, dos,
  config, pifmgr, vdmdbg), sin `wow16` ni `samples`.
- Opciones de build nuevas:
  - `ENABLE_NTVDM` (menú System, `auto`): activa en i386, amd64 y arm64;
    desactivada y oculta en riscv64 y ppc. Verificado: `ENABLE_NTVDM=ON` en
    amd64, ausente del todo en riscv64 y ppc.
  - `ENABLE_NTVDM_BOOT_RUN` (menú Testing, solo Debug), aún sin consumidor;
    el arnés se implementa en S2.
- `subsystems/CMakeLists.txt` añade `mvdm` solo con `ENABLE_NTVDM`, sin
  condición de compilador.
- `asm16` ensambla `COMMAND.COM` con el ensamblador integrado de Clang
  (`clang -target i386-unknown-elf -x assembler`); se eliminó la dependencia
  obligatoria de `i686-w64-mingw32-as` y se comprobó que no aparece en
  `build.ninja`.
- Compilaciones verificadas:
  - Clang amd64 Debug `-Werror`: `ntvdm.exe` (PE32+ x86-64), `command.com`,
    `pifmgr.dll`, `vdmdbg.dll`.
  - GCC i386 y GCC amd64: los cuatro artefactos.
  - Clang arm64: los cuatro artefactos.
- Imágenes amd64 generadas con todos los artefactos: `LiberNT.img`,
  `bootcd.iso` y `LiberNT.vhd` (listas de imagen comprobadas: `ntvdm.exe`,
  `command.com`, `pifmgr.dll`, `vdmdbg.dll`, `CONFIG.NT` y `AUTOEXEC.NT` en
  `system32`, y CONFIG/AUTOEXEC también en `repair`).
- `IMAGEBASE 0x0F000000` eliminada de la definición del módulo `ntvdm`.

## Desviaciones respecto a la especificación (qué y por qué)

1. **Documentación en `docs/fase1/` y no en `docs/` raíz.** Decisión del
   desarrollador en esta sesión: se mantiene la carpeta `fase1/`. El informe y
   el checklist de la fase referencian `docs/…`; los enlaces de `CLAUDE.md` y
   de este informe se leen respecto a `docs/fase1/`. No afecta a ficheros de
   upstream.
2. **Supresiones locales de avisos de Clang en el target `ntvdm`.**
   `-Wno-unused-but-set-global` (aviso nuevo de Clang ≥ 19, no cubierto por el
   `-Wno-unused-but-set-variable` que el árbol ya usa) y `-Wno-multichar` (el
   árbol ya lo silencia en `gcc.cmake` y `msvc.cmake`, pero no en `clang.cmake`).
   Se aplican en `subsystems/mvdm/ntvdm/CMakeLists.txt` (zona del fork) para no
   tocar `sdk/cmake/clang.cmake` y no silenciar de más en el resto del árbol.
3. **`process.c`: firma MZ/ZM como palabra.** Las constantes multi-carácter
   `'MZ'`/`'ZM'` son frágiles y Clang 24 las rechaza con `-Werror`; se
   sustituyen por `0x5A4D`/`0x4D5A`, el mismo conjunto de valores que aceptaba
   el original.
4. **La primera sincronización fue fast-forward, sin PR.** El procedimiento de
   `CLAUDE.md` termina en una PR a `main`; con `main` ya en el commit de
   upstream, la PR no tendría contenido. Pendiente de confirmar con el
   desarrollador si se quiere que la próxima sincronización (con trabajo del
   fork encima) materialice el flujo completo con PR.
5. **`sdk/cmake/config.cmake` y `rosconfig.def`** son ficheros de upstream
   listados en §2.3 del informe, así que los cambios están dentro de lo
   previsto; se citan aquí por trazabilidad.

## Estructura de archivos creada/modificada

**Creados (zona del fork):**

| Fichero | Contenido |
|---|---|
| `LIBRENT_VERSION` | `0.1.0` (versión de la fase, aún en desarrollo) |
| `NOVEDADES.md` | Entrada 0.1.0 en lenguaje de usuario (borrador) |
| `docs/fase1/librent-fase1-implementacion.md` | Este informe |

**Modificados (ficheros de upstream listados en §2.3):**

| Fichero | Cambio |
|---|---|
| `sdk/cmake/config.cmake` | `_ntvdm_default` + `cmake_dependent_option(ENABLE_NTVDM …)` y `cmake_dependent_option(ENABLE_NTVDM_BOOT_RUN …)` |
| `sdk/cmake/rosconfig.def` | `config ENABLE_NTVDM` en el menú System y `config ENABLE_NTVDM_BOOT_RUN` en Testing |
| `subsystems/CMakeLists.txt` | `add_subdirectory(mvdm)` cuando `ENABLE_NTVDM` |

**Restaurados desde `054ce27^` (zona del fork `subsystems/mvdm`, 208 ficheros):**

| Fichero | Cambio local tras la restauración |
|---|---|
| `subsystems/mvdm/CMakeLists.txt` | sin `wow16` ni `samples` |
| `subsystems/mvdm/asm16.cmake` | sin la comprobación que exigía `i686-w64-mingw32-as` |
| `subsystems/mvdm/ntvdm/CMakeLists.txt` | sin `IMAGEBASE`; supresiones de avisos de Clang del punto 2 de desviaciones |
| `subsystems/mvdm/ntvdm/dos/dos32krnl/process.c` | firma MZ/ZM como palabra (desviación 3) |

**Estado del repositorio:** los cambios están en cinco commits (formato de
upstream, sin firma), subidos a `origin`:

| Commit | Asunto |
|---|---|
| `464c9919119` | [DOCS] Add the librent working rules and phase 1 documentation |
| `0ec7e70ee3d` | [LIBRENT] Start the fork at version 0.1.0 |
| `09e788ca218` | [NTVDM] Restore the virtual DOS machine subsystem |
| `655fcafbaf4` | [NTVDM] Build the restored subsystem with Clang and on 64-bit targets |
| `b47b2ba37e2` | [CMAKE] Add the ENABLE_NTVDM build option |

Se subieron `main` (al commit de upstream) y `fase1-ntvdm`, y se abrió la PR
interna [#1](https://github.com/HectorEspejo/librent/pull/1) contra `main`
del fork. `AGENTS.md` sin cambios frente a upstream. Los workflows `stale` y
`labeler` quedaron `disabled_manually`; el workflow Build está activo.

## Decisiones técnicas tomadas durante el desarrollo

- **`asm16` con el ensamblador integrado.** El LLVM actual (24 en RosBE, 22 en
  el sistema) ya emite las relocalizaciones de 16 bits que antes obligaban a
  usar el `as` de i686; se probó primero con el clang del sistema y se confirmó
  en la build real. La ruta GCC conserva `native-obj2bin` y la ruta MSVC queda
  intacta.
- **Patrón de opción `auto` calcado de `ENABLE_WOW64`:** valor por defecto
  calculado en `config.cmake` y expresión `depends` en `rosconfig.def`, con
  `cmake_dependent_option` para que en riscv64/ppc ni exista.
- **Supresión de avisos local al target** en lugar de global: menos superficie
  fuera de la zona del fork y solo afecta al código restaurado.
- **`LIBRENT_VERSION` arranca en `0.1.0`** (la versión objetivo de la fase) y
  `NOVEDADES.md` se irá completando con cada funcionalidad, como pide la regla
  de la casa.
- **Anotación de la sincronización** con fecha y commit de upstream en este
  informe, tanto en el resumen como aquí: 18/10/2026, `97b438262df`.
- **Commits, push y PR hechos** con la confirmación del desarrollador; la PR
  interna es la #1 del fork.

## Funcionalidades del checklist completadas

- Fork en GitHub de `ahmedarif193/LiberNT` creado con `gh` con el nombre `librent`, con la rama `main` creada desde `main-nt10` como rama por defecto
- Remoto `upstream` apuntando a `ahmedarif193/LiberNT` y procedimiento de fusión semanal documentado en `CLAUDE.md`
- Primera fusión de `upstream/main-nt10` hecha con el procedimiento documentado, con fecha y commit de upstream anotados en el informe de implementación
- `gh repo set-default` apuntando al fork, para que ninguna PR vaya a `ahmedarif193/LiberNT`
- `CLAUDE.md` en la raíz y documentación de la fase en `docs/`
- `subsystems/mvdm` restaurado desde `054ce27^` con ntvdm, dos, config, pifmgr y vdmdbg, sin `wow16` ni `samples`
- Opción `ENABLE_NTVDM` en el menú System de menuconfig con valor `auto`: activa en i386, amd64 y arm64, desactivada en riscv64 y ppc
- `subsystems/CMakeLists.txt` incluye `mvdm` solo cuando `ENABLE_NTVDM` está activa, sin condición de compilador
- `asm16` ensambla COMMAND.COM con el ensamblador integrado de Clang, sin `i686-w64-mingw32-as`
- NTVDM compila con Clang y `-Werror` en amd64 Debug
- NTVDM compila con GCC en i386 y amd64
- NTVDM compila con Clang en arm64
- Ningún `#ifdef` de arquitectura nuevo en `subsystems/mvdm` ni en `sdk/lib/fast486`
- `IMAGEBASE` fija eliminada de la definición del módulo `ntvdm`
- `ntvdm.exe`, `command.com`, `pifmgr.dll`, `vdmdbg.dll`, `CONFIG.NT` y `AUTOEXEC.NT` incluidos en `bootcd.iso`, `LiberNT.img` y `LiberNT.vhd`

## Pendientes y bloqueos

- **CI del fork**: los workflows quedaron registrados con el push y `stale` y
  `labeler` están `disabled_manually`; falta ver los 7 trabajos de `build.yml`
  en verde (en el momento de escribir esto no se había disparado ninguna
  ejecución; se está provocando con un push nuevo).
- **S2 en adelante**: memoria reubicable y guarda, `BaseGetVdmConfigInfo` con
  reserva 0 (incluye la guarda en `proc.c` que se anotará como desviación),
  validación de punteros, arnés mínimo (`vmtest.py` + `ENABLE_NTVDM_BOOT_RUN` +
  `vmtestmark` + `hello.com`) y primer arranque real en QEMU/KVM.
- **Nota O-01**: sigue sin confirmar por el desarrollador; si cambia, afecta a
  pruebas y plan, no a lo ya implementado.
- **Estado de ppc/riscv64**: solo se verificó la configuración (sin NTVDM);
  no se compilan imágenes para esas arquitecturas en esta fase.

## Ejecución y pruebas (cómo arrancar, migrar y testear)

```sh
. "$HOME/.local/opt/rosbe/rosbe-env.sh"
export CCACHE_DISABLE=1

# Builds usadas en esta sesión
./configure.sh -a amd64                 # Clang, Debug → output-Clang-amd64-debug
ninja -C output-Clang-amd64-debug ntvdm command pifmgr vdmdbg
ninja -C output-Clang-amd64-debug liberntimg bootcd liberntvhd

./configure.sh -a i386 --gcc            # → output-GCC-i386-debug
ninja -C output-GCC-i386-debug ntvdm command pifmgr vdmdbg

./configure.sh -a amd64 --gcc           # → output-GCC-amd64-debug
ninja -C output-GCC-amd64-debug ntvdm command pifmgr vdmdbg

./configure.sh -a arm64                 # → output-Clang-arm64-debug
ninja -C output-Clang-arm64-debug ntvdm command pifmgr vdmdbg

./configure.sh -a riscv64               # comprobación del auto-off
./configure.sh -a ppc                   # comprobación del auto-off
```

- Arranque de la imagen en QEMU/KVM (pantalla, para inspección manual):

```sh
qemu-system-x86_64 -enable-kvm -m 2048 -drive file=output-Clang-amd64-debug/LiberNT.img,format=raw
```

- Pruebas automáticas: aún no disponibles; llegan con el arnés de S2
  (`scripts/librent/vmtest.py payload|run|report|all`).
- Migración a otra máquina: clonar el repo, instalar RosBE con el bootstrap,
  `mtools` y Python con `mako/packaging/pyyaml`, y ejecutar las mismas órdenes.
