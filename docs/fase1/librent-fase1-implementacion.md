# librent - Fase 1: NTVDM en 64 bits (DOS en modo texto)

## Informe de implementación

**Última actualización:** 18/10/2026 (sesión 2: S2)

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

### S2 · 64 bits y primer arranque

- **Memoria del invitado reubicable**: `MemInitialize` reserva 16 MB + 64 KB de
  guarda en cualquier dirección (`BaseAddress` nunca 0) y confirma solo los
  16 MB con `PAGE_EXECUTE_READWRITE`; la guarda queda reservada e inaccesible.
  Evidencia en el log serie del arnés: `NTVDM: Guest memory at
  000000BB25610000, guard at 000000BB26610000`.
- **Validación de conversiones invitado→host**: nueva
  `MemIsValidAddress(Address, Size)` (verificación de rango contra
  `MAX_ADDRESS`, sin desbordes). Se aplica en la E/S de ficheros DOS
  (`DosReadFile`/`DosWriteFile`), en las transferencias del BIOS de disco
  (`ReadDisk`/`WriteDisk`) y en `MGetVdmPointer` (ruta BOP/VDD).
- **`BaseGetVdmConfigInfo`**: devuelve reserva 0 y falla con
  `ERROR_BAD_EXE_FORMAT` si `system32\ntvdm.exe` no existe. La línea de
  comandos usa la ruta ya compuesta del ejecutable. En `proc.c` se añadió la
  guarda para que el contador de reserva no desborde con tamaño 0.
- **Emparejamiento VDM ↔ consola (riesgo R-02)**: con condrv los valores de
  handle de consola difieren entre procesos (cmd 0x2C, ntvdm 0x04), así que la
  búsqueda por handle fallaba con `ERROR_NOT_FOUND`. Ahora basesrv asigna un
  identificador de sesión a **toda** VDM (no solo a las sin consola), kernel32
  lo pasa siempre con `-i<sesión>` y la VDM se identifica por sesión; además
  basesrv registra el PID de la VDM y lo usa como respaldo en las llamadas que
  solo llevan handle de consola (`ExitVDM`, contador de reentrada, directorios
  actuales). `dem.c` distingue «tiene consola» (para el arranque del COMSPEC
  de 32 bits y el título) del identificador de sesión.
- **VDM efímera**: corregido el tratamiento de `VDM_FLAG_DONT_WAIT` en basesrv
  (antes devolvía igualmente el evento de espera, y la VDM quedaba bloqueada
  hasta la siguiente orden). Ahora, al terminar un programa sin órdenes
  pendientes, la VDM sale; cada orden nueva lanza su propia VDM. Verificado:
  dos ejecuciones seguidas dejan dos VDM que terminan, sin residuos.
- **Redirección**: se eliminó el forzado `bInheritHandles = FALSE` de la ruta
  VDM en `proc.c`, de modo que la VDM hereda los handles estándar de cmd y la
  E/S DOS se enlaza con el fichero o tubería de la orden.
- **Arnés de pruebas**:
  - `scripts/librent/vmtest.py` (Python 3, solo biblioteca estándar) con las
    órdenes `payload`, `run`, `report` y `all`.
  - `ENABLE_NTVDM_BOOT_RUN` instala `vmtestmark.exe` y
    `librent-vmtest.cmd` en `system32` y registra la entrada `Run` de HKCU;
    busca `\LIBRENT\RUNTESTS.CMD` de C: a Z: y lo ejecuta.
  - Añade `/DEBUG /DEBUGPORT=COM1 /BAUDRATE=115200` a las opciones de arranque
    de la imagen preinstalada, para que las marcas del arnés lleguen al log
    serie.
  - Programas propios `hello.com` y `oobread.com` (ensamblador integrado de
    Clang) y `vmtestmark.exe` en `modules/rostests/tests/ntvdm16/`.
  - Disco de pruebas FAT32 creado con `mtools`; QEMU/KVM sin pantalla, serie a
    fichero, QMP y el disco de pruebas como segunda unidad IDE.
  - **Resultado: `vmtest.py all` en verde (2/2)** con hello.com redirigido y
    oobread.com en el borde de la memoria direccionable.
- Comprobación de `VdmAllowed` mantenida en `wmain`; `ntoskrnl/nvs` sin
  cambios.

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
6. **`dll/win32/kernel32/client/proc.c`** (fichero no listado en §2.3): dos
   cambios mínimos exigidos por la fase. (a) La guarda `if (VdmReserve)
   VdmReserve--;` en las dos ramas VDM, sin la cual el contador desborda si
   `BaseGetVdmConfigInfo` devuelve reserva 0. (b) Se deja de forzar
   `bInheritHandles = FALSE` en la rama DOS para que la redirección de cmd
   llegue a la VDM (sin ello, la salida DOS va siempre al CON).
7. **`subsystems/win/basesrv/vdm.c`** (fichero no listado en §2.3): asignación
   de identificador de sesión a toda VDM, emparejamiento de respaldo por PID
   del cliente y corrección de `VDM_FLAG_DONT_WAIT` (antes devolvía el evento
   de espera aunque el llamante pidiera respuesta inmediata, lo que impedía la
   VDM efímera). Son cambios de contrato interno CSR, no de ABI pública.
8. **`boot/boot_images.cmake`** (fichero contemplado en §2.3 como «fichero que
   activa las opciones `*_BOOT_RUN`»): con `ENABLE_NTVDM_BOOT_RUN` añade
   `/DEBUG /DEBUGPORT=COM1 /BAUDRATE=115200` a las opciones de arranque de la
   imagen preinstalada mediante el mecanismo existente
   `FREELDR_PREINSTALL_OPTIONS`.
9. **Arnés dentro de la zona del fork**: el subdirectorio de pruebas se añade
   desde `subsystems/mvdm/CMakeLists.txt` cuando `ENABLE_NTVDM_BOOT_RUN` está
   activa, sin tocar `modules/rostests/tests/CMakeLists.txt` (upstream).
10. **AC de validación de punteros no ejercitable en modo real.** La
    comprobación «INT 21h lee en un búfer que cruza el final de la memoria del
    invitado» no es alcanzable desde código de 16 bits: con segmento y
    desplazamiento de 16 bits la dirección lineal máxima es `0x10FFEF` y el
    contador máximo `0xFFFF`, así que ningún búfer real-mode puede acercarse a
    los 16 MB (`0x1000000`). La validación está implementada y en uso (DOS,
    BIOS y ruta BOP/VDD), y `oobread.com` comprueba el borde alcanzable
    (`FFFF:FFF0` + 32 bytes). Se pone en conocimiento del analista para que
    confirme la AC o la reformule; una ejercitación negativa requeriría una
    interfaz de 32 bits (Fase 3/DPMI).
11. **`vmtest.py run` usa instantánea por unidad** (`snapshot=on` en el disco
    del sistema, el de pruebas sin instantánea) en lugar del `-snapshot`
    global, que descartaría también los `TEST_RESULT` escritos en el disco de
    pruebas. Es la intención del criterio (no modificar `LiberNT.img`).
12. **`subsystems/mvdm/ntvdm/console/console.c`**: los bucles
    `while (ShowConsoleCursor(...) >= 0) ;` del frontend de consola heredado
    bloqueaban el arranque de la VDM (el stub actual de `ShowConsoleCursor`
    devuelve −1 y el bucle «mostrar cursor» no terminaba nunca). Se sustituyen
    por una llamada única. Este fichero desaparece en S3 con el frontend de
    ventana, así que el cambio es un puente mínimo para poder validar S2.

## Estructura de archivos creada/modificada

**Creados (zona del fork):**

| Fichero | Contenido |
|---|---|
| `LIBRENT_VERSION` | `0.1.0` (versión de la fase, aún en desarrollo) |
| `NOVEDADES.md` | Entrada 0.1.0 en lenguaje de usuario (borrador) |
| `docs/fase1/librent-fase1-implementacion.md` | Este informe |
| `scripts/librent/vmtest.py` | Arnés: `payload`, `run`, `report`, `all` |
| `scripts/librent/librent-vmtest.cmd` | Busca `\LIBRENT\RUNTESTS.CMD` de C: a Z: y lo ejecuta |
| `scripts/librent/corpus/manifest.json` | `CORPUS_ITEM` propios (hello, oobread) |
| `scripts/librent/corpus/golden/own-hello.out`, `own-oobread.out` | Salidas de referencia |
| `modules/rostests/tests/ntvdm16/CMakeLists.txt` | hello.com, oobread.com, vmtestmark.exe |
| `modules/rostests/tests/ntvdm16/hello.S`, `oobread.S` | Programas de 16 bits |
| `modules/rostests/tests/ntvdm16/vmtestmark.c` | Marcas `LIBRENT-VMTEST-KEYS`/`-DONE` |
| `modules/rostests/tests/ntvdm16/librent-vmtest.inf` | Entrada `Run` de HKCU |

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

**Modificados en S2 (fuera de §2.3, desviaciones 6–9):**

| Fichero | Cambio |
|---|---|
| `dll/win32/kernel32/client/vdm.c` | reserva 0 y `ERROR_BAD_EXE_FORMAT` si falta `ntvdm.exe` |
| `dll/win32/kernel32/client/proc.c` | guarda de `VdmReserve`; herencia de handles en la rama DOS |
| `subsystems/win/basesrv/vdm.c` | sesión por VDM, respaldo por PID, `DONT_WAIT` |
| `boot/boot_images.cmake` | opciones de depuración de la imagen con `ENABLE_NTVDM_BOOT_RUN` |
| `subsystems/mvdm/CMakeLists.txt` | añade `modules/rostests/tests/ntvdm16` con el arnés |
| `subsystems/mvdm/ntvdm/memory.c`, `emulator.h` | memoria reubicable, guarda, `MemIsValidAddress` |
| `subsystems/mvdm/ntvdm/dos/dos32krnl/dosfiles.c` | validación del búfer DOS |
| `subsystems/mvdm/ntvdm/hardware/disk.c` | validación del búfer del BIOS de disco |
| `subsystems/mvdm/ntvdm/dos/dem.c` | `HasConsole`, `TaskId` de sesión, VDM efímera |
| `subsystems/mvdm/ntvdm/console/console.c` | sin bucles de reintento infinitos de `ShowConsoleCursor` (desviación 12) |

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

Los cambios de S2 están en el árbol de trabajo, **sin commitear**, a la espera
de la confirmación del desarrollador.

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
- **Memoria del invitado reubicable**: la reserva se hace con base `NULL` y se
  exige que el sistema no la sitúe en 0; la guarda de 64 KB se reserva en la
  misma operación y solo se confirman los 16 MB, de modo que queda inaccesible
  sin necesidad de protegerla después.
- **Identificación de la VDM por sesión, no por handle de consola**: con
  condrv los valores de handle dejan de ser comparables entre procesos
  (hecho comprobado: 0x2C en cmd y 0x04 en ntvdm). Se mantiene el handle para
  lo que sí es válido (los registros por consola del proceso que lanza) y se
  usa la sesión (y el PID como respaldo) para las llamadas de la propia VDM.
- **«Tiene consola» como concepto separado del identificador de sesión**:
  `dem.c` decide con `GetConsoleWindow()` si debe arrancar el COMSPEC de 32
  bits y poner título, en lugar de con `SessionId != 0`, que ahora siempre es
  cierto en la práctica.
- **VDM efímera estricta**: al terminar un programa sin órdenes pendientes la
  VDM sale y cmd lanza otra para la siguiente orden. Es lo que da redirección
  correcta por orden (cada VDM hereda los handles de su comando) y cumple la
  promesa de no dejar procesos residentes.
- **Validación central de direcciones** (`MemIsValidAddress`) en lugar de
  comprobaciones ad hoc: se aplica en los tres puntos de conversión masiva
  (DOS, BIOS y BOP/VDD) y devuelve error del invitado sin tocar memoria del
  host.
- **El subdirectorio de pruebas se engancha desde `subsystems/mvdm`** (zona
  del fork) y solo con `ENABLE_NTVDM_BOOT_RUN`, para no tocar el árbol de
  tests de upstream y no engordar las imágenes normales.
- **Las marcas del arnés se emiten con `OutputDebugStringA`** y llegan a COM1
  gracias a `/DEBUG /DEBUGPORT=COM1 /BAUDRATE=115200`, horneado en la imagen
  solo cuando `ENABLE_NTVDM_BOOT_RUN` está activa.

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
- Memoria del PC emulado (16 MB) reservada en cualquier dirección del proceso, nunca en la dirección 0
- `BaseGetVdmConfigInfo` devuelve una reserva de tamaño 0 y kernel32 no reserva nada en el proceso hijo
- `ntoskrnl/nvs` sin ningún cambio
- 64 KB de guarda sin acceso detrás de la memoria del invitado
- Conversiones de dirección del invitado a puntero del host (BOP, BIOS y DOS) validadas contra el tamaño de la memoria del invitado
- Comprobación de `VdmAllowed` (`ProcessWx86Information`) mantenida al arrancar `ntvdm.exe`
- `scripts/librent/vmtest.py` en Python 3 sin dependencias externas, con las órdenes `payload`, `run`, `report` y `all`
- `vmtest.py run` arranca `LiberNT.img` en QEMU/KVM sin pantalla, con `-snapshot`, salida serie a fichero, QMP y el disco de pruebas como segunda unidad
- Opción `ENABLE_NTVDM_BOOT_RUN` (solo Debug) que al iniciar sesión busca `\LIBRENT\RUNTESTS.CMD` de C: a Z: y lo ejecuta
- `vmtestmark.exe` emite las marcas `LIBRENT-VMTEST-KEYS` y `LIBRENT-VMTEST-DONE` por la salida de depuración hacia el puerto serie
- `vmtest.py report` compara con las referencias, imprime una tabla y sale con error si alguna prueba falla
- Un `.COM` o `.EXE` de DOS lanzado desde cmd se ejecuta en una VDM y cmd espera a que termine
- VDM efímera: ejecuta las órdenes pendientes de su consola y termina cuando no le quedan, sin quedar residente
- COMMAND.COM integrado ejecuta el programa pedido y devuelve su código de salida
- `vmtest.py payload`
- `vmtest.py run`
- `vmtest.py report`
- `vmtest.py all`

## Pendientes y bloqueos

- **CI del fork**: los workflows quedaron registrados con el push y `stale` y
  `labeler` están `disabled_manually`. En la primera ejecución de `build.yml`
  (run `39159359232`) cinco de los siete trabajos ya están en verde (GCC i386,
  GCC amd64, Clang riscv64 ×2 y Clang ppc); Clang amd64 y Clang arm64 seguían
  en curso al cerrar la sesión. Queda confirmarlos y anotar el resultado.
- **Decisión del analista sobre la AC de validación de punteros** (desviación
  10): la comprobación literal no es alcanzable desde modo real de 16 bits.
  Opciones: reformular la AC (p. ej., validar el borde `FFFF:FFF0`), moverla a
  la Fase 3 (DPMI) o dejar `oobread.com` como comprobación de no rechazo en el
  borde.
- **Programas de prueba restantes** (`exitcode`, `stdio`, `filetest`,
  `memtest`, `xmstest`, `emstest`, `mousetest`, `speaker`, `idle`, `keytest`,
  `textmodes`, `bench16`) y marcas `LIBRENT-VMTEST-KEYS` por QMP: S5.
- **Código de salida distinto de 0**: el mecanismo está probado con 0; el caso
  `exitcode.com 7` y ERRORLEVEL llegan con la prueba `exitcode` (S5).
- **`ENABLE_NTVDM` desactivada**: `ERROR_BAD_EXE_FORMAT` implementado; falta
  la prueba con una imagen sin `ntvdm.exe` (S4/S5).
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

- Arnés de pruebas (S2). La imagen debe estar construida con el arnés activado:

```sh
./configure.sh -a amd64 -DENABLE_NTVDM_BOOT_RUN=ON
ninja -C output-Clang-amd64-debug liberntimg
python3 scripts/librent/vmtest.py all \
    --image output-Clang-amd64-debug/LiberNT.img \
    --build-dir output-Clang-amd64-debug
```

Resultado esperado y comprobado: `own-hello ok`, `own-oobread ok`,
`2/2 items ok`. El trabajo del arnés vive en `output-Clang-amd64-debug/vmtest/`
(`payload.img`, `results/`, `serial.log`, `qmp.sock`), se reutiliza y se
sobrescribe en cada ejecución.

Órdenes por separado: `payload` (crea el disco de pruebas FAT32 con `mtools`),
`run` (arranca QEMU/KVM sin pantalla con instantánea del disco del sistema y
el disco de pruebas como segunda unidad IDE), `report` (extrae `RESULTS\` y
compara con `scripts/librent/corpus/golden/`) y `all` (las tres).

- Migración a otra máquina: clonar el repo, instalar RosBE con el bootstrap,
  `mtools` y Python con `mako/packaging/pyyaml` (solo para configurar Mesa), y
  ejecutar las mismas órdenes.
