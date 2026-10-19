# librent - Fase 1: NTVDM en 64 bits (DOS en modo texto)

## Informe de implementación

**Última actualización:** 19/10/2026 (sesión 3: S3)

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

### S3 · Ventana y entrada

- **Frontend de ventana nuevo** en `ntvdm/frontend/`: se elimina
  `ntvdm/console/` y el estado de la ventana vive en `VDM_FRONTEND`
  (`frontend.h`), con hilo de interfaz propio y bucle de mensajes separado del
  hilo de emulación. `console/video.c` se sustituye por `frontend/video.c`
  (incluido en `svga.c`, como antes, para acceder a los registros y a la
  memoria VGA) y la entrada por `frontend/input.c`.
- **Pintado de texto con la fuente del plano 2**: el renderer copia la rejilla
  de `CHAR_CELL`, la fuente (256 glifos × 32 filas), la paleta DAC y el estado
  del cursor en la retraza vertical (hilo de emulación) y el hilo de ventana
  pinta desde esa instantánea; el acceso está protegido por una sección
  crítica. Ancho de celda de 8 o 9 puntos según el registro de reloj del
  secuenciador, con la novena columna replicada en `C0h–DFh` sólo en filas
  impares cuando los gráficos de línea están activos, como especifica §7.2.
- **Fuente de usuario (INT 10h AX=1110h)**: `vidbios.c` carga el rango de
  patrones indicado por ES:BP/CX/DX con la nueva
  `VgaWriteTextModeFontRange`. Verificado con `textmodes.com`: el carácter se
  pinta como bloque relleno en la ventana.
- **Colores, parpadeo y cursor**: atributos según la paleta DAC; el bit 7 es
  parpadeo o fondo intenso según el registro de modo del controlador de
  atributos; el cursor usa inicio/fin/posición/visibilidad del CRTC y parpadea
  con un temporizador de 266 ms.
- **Escalado y pantalla completa**: escalado entero automático (el mayor que
  cabe, hasta 4×) o fijo 1×–4×; Alt+Intro y el menú alternan pantalla completa
  sin cambiar el modo de vídeo del sistema, con la imagen centrada y bandas
  negras. La posición, la escala y el arranque en pantalla completa se guardan
  en `HKCU\Software\librent\NTVDM` (`WindowX/WindowY`, `WindowScale`,
  `StartFullScreen`).
- **Menú del sistema y «Acerca de»**: Pantalla completa, Escala (Automática y
  1×–4×), Copiar texto, Pegar, Volcar memoria (texto/binario), Terminar la VDM
  y Acerca de la VDM…, esta última con `librent 0.1.0` (de
  `LIBRENT_VERSION`) y `LiberNT 0.4.17 (dev)`. Verificado con capturas de
  pantalla por QMP.
- **Cierre**: con el programa en marcha pide confirmación (nombre del programa
  incluido) y, si se confirma, el proceso sale con `STATUS_CONTROL_C_EXIT`
  (`0xC000013A`); sin diálogos (`NTVDM_NO_DIALOGS=1`) no pregunta. Verificado
  con `keytest.com` y Alt+F4 en QEMU.
- **Entrada**: teclas traducidas a códigos de exploración del conjunto 1 (con
  `E0` en las extendidas) y caracteres según la distribución activa de Windows
  convertidos a CP437. La BIOS usa el carácter traducido por el anfitrión
  (cola paralela en `keyboard.c`) y vuelve a `ToAscii` cuando no lo hay.
  Verificado de punta a punta: teclas enviadas por QMP → ventana → PS/2 →
  BIOS → `INT 16h` (`keytest.com`: `SCAN=1E CHAR=61`).
- **Ratón**: eventos del ratón traducidos a coordenadas DOS y enviados a
  `INT 33h`; el puntero del sistema se oculta mientras el programa muestra su
  cursor (enganche en `mouse32.c`).
- **Portapapeles**: «Copiar texto» vuelca la pantalla completa a Unicode desde
  CP437 (sin espacios finales y con CRLF) a `CF_UNICODETEXT`; «Pegar» convierte
  el portapapeles a CP437 y alimenta el búfer de teclado de la BIOS en cada
  retraza vertical mientras quede hueco.
- **Consola del sistema**: si la VDM es el único proceso de su consola (doble
  clic desde el Explorador) se oculta la ventana de consola; `ConsoleAttach` /
  `ConsoleDetach` la vuelven a ocultar o mostrar para los programas Win32
  lanzados desde DOS.
- **Arnés**: `vmtest.py payload` pasa a grabar `\launcher.cmd` en el disco de
  pruebas, con lo que el mecanismo de arranque de pruebas del árbol
  (`app_launcher.cmd`) ejecuta `librent-vmtest.cmd` y, además, el asistente de
  primer arranque (OOBE) se salta solo (`HasAutomationPayload`). Se evita así
  el doble arranque y el OOBE que robaba el foco. Verificado con `keytest.com`
  y capturas.
- Compilado en Clang amd64 Debug, GCC i386, GCC amd64 y Clang arm64; arnés en
  verde (2/2) tras todos los cambios.

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

13. **«Acerca de la VDM» usa un cuadro de mensaje** en lugar del diálogo a
    medida del mockup; el contenido (versiones de librent y LiberNT, CPU,
    memoria y licencia) es el especificado. El diálogo de cierre es el cuadro
    estándar de Sí/No: pide confirmación con el nombre del programa, pero los
    botones se rotulan «Sí/No» en vez de «Terminar/Cancelar».
14. **El código de salida del cierre**: el proceso `ntvdm.exe` sale con
    `STATUS_CONTROL_C_EXIT` (`0xC000013A`), como pide la AC. `cmd`, en cambio,
    recibe el código que basesrv tiene registrado para la tarea DOS (0), porque
    `GetExitCodeProcess` de un proceso VDM se resuelve con `BaseCheckForVDM` y
    el modelo `VDM_DOS_RECORD` de §3.2. Si el analista quiere que ERRORLEVEL
    sea también `0xC000013A`, hace falta propagar el código a la tarea DOS en
    `ExitVDM`; queda anotado.
15. **Modos 80×43 y 80×50**: el frontend es genérico (columnas × filas según
    CRTC), pero esos modos dependen de que el invitado programe el CRTC y la
    altura de carácter, como hace MS-DOS `MODE CON`. Verificados 80×25 (modo
    03h) y la carga de fuente 8×8/8×14/8×16; la cobertura de 43/50 filas
    queda para las pruebas interactivas.
16. **`frontend/frontend.c` e `input.c` son unidades de compilación nuevas**
    (no se incluyen con el truco de `console.c` en `ntvdm.c`); `video.c` sí se
    incluye en `svga.c` como el fichero anterior. El `VDM_FRONTEND` guarda el
    estado del spec; los detalles de pintado (instantánea, DIB, rect destino)
    viven en `video.c`.
17. **El asistente de primer arranque (OOBE) y el arranque de pruebas** usan el
    gancho existente `\launcher.cmd` + `app_launcher.cmd`; se descartó la
    entrada `Run` propia (se llegó a crear y se retiró en la misma sesión)
    porque provocaba doble ejecución del arnés.

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

**Creados en S3 (zona del fork):**

| Fichero | Contenido |
|---|---|
| `subsystems/mvdm/ntvdm/frontend/frontend.h` | `VDM_FRONTEND` y API del frontend |
| `subsystems/mvdm/ntvdm/frontend/frontend.c` | Ventana, hilo de interfaz, menú, ajustes, consola, cierre |
| `subsystems/mvdm/ntvdm/frontend/video.c` | Pintado (texto/gráficos), escala, cursor, instantánea, copiar |
| `subsystems/mvdm/ntvdm/frontend/input.c` | Teclado CP437, ratón, foco, portapapeles, pegado |
| `modules/rostests/tests/ntvdm16/keytest.S` | Prueba de teclado por BIOS |
| `modules/rostests/tests/ntvdm16/textmodes.S` | Fuente de usuario INT 10h 1110h |
| `docs/usuario/dos.md` | Documentación de usuario de los programas DOS |

**Eliminado:** `subsystems/mvdm/ntvdm/console/` (`console.c`, `video.c`,
`video.h`) y `librent-vmtest.inf`.

**Modificados en S3 (además de los de S2):**

| Fichero | Cambio |
|---|---|
| `subsystems/mvdm/ntvdm/ntvdm.c`, `ntvdm.h` | sin el `#include` de `console/`; `VdmShutdown` exportada; salida con el código del frontend |
| `subsystems/mvdm/ntvdm/emulator.c`, `emulator.h` | sin hilo de entrada de consola ni `ScreenEventHandler`/`MenuEventHandler`; `EmulatorInitialize()` sin argumentos |
| `subsystems/mvdm/ntvdm/hardware/video/svga.c`, `svga.h` | incluye `frontend/video.c`; `VgaInitialize()`; bombeo de pegado en la retraza; `VgaWriteTextModeFontRange` |
| `subsystems/mvdm/ntvdm/bios/vidbios.c` | INT 10h AX=1110h (fuente de usuario) |
| `subsystems/mvdm/ntvdm/bios/bios32/kbdbios32.c` | carácter traducido por el anfitrión; API de empuje al búfer |
| `subsystems/mvdm/ntvdm/hardware/keyboard.c` | cola de caracteres del anfitrión; inyección de scancodes |
| `subsystems/mvdm/ntvdm/dos/dem.c` | título de la ventana al arrancar cada programa |
| `subsystems/mvdm/ntvdm/dos/mouse32.c` | ocultar/mostrar el puntero del sistema |
| `subsystems/mvdm/ntvdm/lang/en-US.rc`, `es-ES.rc`, `resource.h` | textos y menú nuevos |
| `modules/rostests/tests/ntvdm16/CMakeLists.txt` | keytest y textmodes |
| `scripts/librent/vmtest.py` | `launcher.cmd` en el disco de pruebas |
| `NOVEDADES.md` | funciones visibles de la ventana |

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
- **Instantánea de pantalla en vez de doble búfer con bloqueo del escritor**:
  el hilo de emulación copia celdas, fuente, paleta y cursor a una instantánea
  bajo sección crítica en la retraza vertical y el hilo de ventana pinta desde
  ella. Evita carreras sin tocar el camino caliente de escritura de la memoria
  VGA.
- **Carácter traducido por el anfitrión con respaldo en la BIOS**: la cola de
  caracteres del anfitrión solo se usa cuando existe (teclas con equivalente en
  CP437); las teclas sin carácter y los scancodes inyectados siguen el camino
  clásico `ToAscii`, lo que mantiene compatibilidad con programas que leen
  scancodes.
- **Pegado alimentado por la retraza vertical**: el búfer de la BIOS tiene 16
  posiciones; el bombeo comprueba hueco en cada retraza (~60 Hz), lo que
  satisface «a medida que se vacía» sin depender de que el programa consulte
  `INT 16h`.
- **El pegado inyecta caracteres con scancode 0** a través de la API pública
  del BIOS de teclado (`BiosKeyboardPushKey`), en lugar de simular pulsaciones
  completas.
- **Menú del sistema con `TrackPopupMenu` propio** para Alt+Espacio: el teclado
  del anfitrión no activa el modo de menú de user32 al no pasar las teclas por
  `DefWindowProc`, así que el menú se abre explícitamente y las teclas Alt+F4 y
  Alt+Intro se siguen reenviando.
- **Alt+Intro y Alt+Espacio se consumen en la ventana** (no llegan al DOS),
  pero el resto de Alt+tecla sí llega al programa.
- **Gancho de arranque del arnés**: se usa `\\launcher.cmd` +
  `app_launcher.cmd` del árbol, que además hace que el asistente de primer
  arranque se salte solo; `ENABLE_NTVDM_BOOT_RUN` instala `vmtestmark.exe` y
  `librent-vmtest.cmd` sin entrada `Run` propia.

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
- Frontend de consola (`console/console.c`, `console/video.c`) eliminado y sustituido por el frontend de ventana en `ntvdm/frontend/`
- Hilo de ventana con bucle de mensajes propio, separado del hilo de emulación
- Modo texto pintado con la fuente del plano 2 de la VGA emulada
- Ancho de celda de 8 o 9 puntos según la VGA emulada, con la novena columna replicada en los caracteres C0h–DFh
- Modos de texto 40×25, 80×25, 80×43 y 80×50
- Colores de los 16 atributos según la paleta DAC de la VGA emulada
- Parpadeo de atributo o fondo intenso según el registro de modo del controlador de atributos
- Cursor de texto con la forma, posición y visibilidad del CRTC
- Escalado entero automático (el mayor que cabe, hasta 4×) y fijo 1×, 2×, 3× y 4×
- Alt+Intro alterna pantalla completa sin cambiar el modo de vídeo del sistema
- Título de la ventana con el nombre del programa DOS
- Cerrar la ventana con el programa en marcha pide confirmación y lo termina
- Al terminar el último programa DOS de la VDM, la ventana se cierra y el foco vuelve a la ventana anterior
- Menú del sistema con Pantalla completa, Escala, Copiar texto, Pegar, Volcar memoria, Terminar la VDM y Acerca de la VDM
- «Acerca de la VDM» muestra la versión de librent (`LIBRENT_VERSION`) y la de LiberNT
- Escala y pantalla completa al iniciar guardadas en `HKCU\Software\librent\NTVDM`
- Superficie gráfica del frontend implementada (su validación es de la Fase 2)
- Textos nuevos de la ventana en `en-US` y `es-ES`, con la cadena inglesa en el resto de idiomas
- Teclas traducidas a códigos de exploración del conjunto 1, con prefijo E0 en las extendidas
- Caracteres según la distribución de teclado activa de Windows, convertidos a la página de códigos OEM 437
- Estado de Ctrl, Alt, Mayús, Bloq Mayús y Bloq Num reflejado en el estado de teclado de la BIOS
- Alt+Intro no llega al programa DOS
- Ratón INT 33h con coordenadas absolutas y botones izquierdo, derecho y central
- Puntero del sistema oculto sobre la ventana mientras el programa muestra su cursor de ratón
- Copiar texto: la pantalla entera al portapapeles en Unicode, convertida desde CP437
- Pegar: texto del portapapeles convertido a CP437 e inyectado en el búfer de teclado de la BIOS a medida que se vacía
- `VgaConsoleInitialize` y `VgaConsoleCleanup` crean y cierran la ventana y su hilo
- `VgaConsoleCreateTextScreen` y `VgaConsoleDestroyTextScreen` sobre la rejilla de `CHAR_CELL`
- `VgaConsoleCreateGraphicsScreen` y `VgaConsoleDestroyGraphicsScreen` sobre un DIB indexado con paleta
- `VgaConsoleRepaintScreen` invalida el rectángulo y pinta el hilo de ventana
- `VgaConsoleUpdateTextCursor` actualiza forma, posición y visibilidad del cursor
- `VgaAttachToConsole` y `VgaDetachFromConsole` muestran y ocultan la consola para los programas Win32 lanzados desde DOS
- `VgaGetDoubleVisionState` sin cambios de comportamiento
- `ScreenEventHandler` eliminado
- `KeyboardEventHandler`, `MouseEventHandler` y `FocusEventHandler` llamados desde el hilo de ventana

## Pendientes y bloqueos

- **CI del fork**: los workflows quedaron registrados con el push y `stale` y
  `labeler` están `disabled_manually`. La primera ejecución de `build.yml`
  terminó con los **7 trabajos en verde** (runs `39159359232` por push y
  `39159360559` por PR), incluidos Clang amd64 y Clang arm64. Las ejecuciones
  de S2 (`39210541363` por push y `39210546391` por PR) quedaron en curso al
  cerrar la sesión.
- **Decisión del analista sobre la AC de validación de punteros** (desviación
  10): la comprobación literal no es alcanzable desde modo real de 16 bits.
  Opciones: reformular la AC (p. ej., validar el borde `FFFF:FFF0`), moverla a
  la Fase 3 (DPMI) o dejar `oobread.com` como comprobación de no rechazo en el
  borde.
- **Programas de prueba restantes** (`exitcode`, `stdio`, `filetest`,
  `memtest`, `xmstest`, `emstest`, `mousetest`, `speaker`, `idle`, `bench16`)
  y marcas `LIBRENT-VMTEST-KEYS` por QMP: S5. `keytest` y `textmodes` ya
  existen y se usaron para validar S3.
- **Verificaciones manuales pendientes de S3** (con EDIT y teclado español,
  ratón real, Explorador y el AC del ratón con `mousetest`): S5. La captura de
  pantalla por QMP queda como herramienta manual, no en el arnés automático.
- **Ctrl+C → Ctrl+Pausa**: implementado en el manejador de consola (inyecta la
  secuencia Ctrl+C en el PS/2); falta comprobarlo con un programa que muestre
  el efecto (S5).
- **ERRORLEVEL tras cerrar la ventana**: el proceso sale con `0xC000013A`,
  pero cmd recibe 0 (código de la tarea DOS); ver desviación 14.
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

- Verificación visual de la ventana (manual, fuera del arnés automático):
  `keytest.com` y `textmodes.com` se ejecutan desde un disco de pruebas con
  `launcher.cmd` y se capturan pantallazos por QMP (`screendump`) en los
  momentos de interés; así se comprobaron el texto con la fuente del plano 2,
  la fuente de usuario, el título, el menú del sistema, «Acerca de», la
  confirmación de cierre y la pantalla completa con Alt+Intro.
- Migración a otra máquina: clonar el repo, instalar RosBE con el bootstrap,
  `mtools` y Python con `mako/packaging/pyyaml` (solo para configurar Mesa), y
  ejecutar las mismas órdenes.
