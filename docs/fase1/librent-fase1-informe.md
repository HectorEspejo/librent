# librent - Fase 1: NTVDM en 64 bits (DOS en modo texto)

## Especificación Funcional

**Versión:** 1.0
**Fecha:** 17/10/2026
**Cliente:** Proyecto personal (Hector)
**Base:** LiberNT `main-nt10`, commit `db96bae` (17/10/2026)

> **Supuesto O-01.** La pregunta 12 del descubrimiento (pruebas y entorno) quedó sin respuesta. Se aplica la opción recomendada (12a). Si cambia, solo se tocan §4.5, §5.6, §5.7, §11 y la sección «Pruebas y rendimiento» del checklist.

---

## 1. Visión General

### 1.1 Descripción

librent es un fork propio de LiberNT, el sistema operativo libre de arquitectura NT derivado de ReactOS. Esta primera fase recupera NTVDM, la máquina DOS virtual que LiberNT eliminó el 12/10/2026, y la hace funcionar en sistemas de 64 bits.

Al terminar la fase, un `.COM` o `.EXE` de DOS en modo real se ejecuta con doble clic o desde cmd, sin máquina virtual, en una ventana propia. Es justo lo que Windows de 64 bits no hace: Microsoft documenta que sus versiones x64 y ARM no ejecutan código x86 de 16 bits.

La fase se limita a programas de modo texto. Gráficos, sonido y juegos llegan en la Fase 2; los extensores DOS, en la 3; Windows 3.x, en la 4.

### 1.2 Objetivos

1. Restaurar NTVDM y compilarlo con Clang y GCC en i386, amd64 y arm64, detrás de la opción `ENABLE_NTVDM`.
2. Sacar la memoria del PC emulado de la dirección 0 (base reubicable) para que funcione con el gestor de memoria NVS, sin tocar el kernel.
3. Sustituir el frontend de consola, que dependía de APIs que ya no existen, por una ventana propia: texto con la fuente de la VGA, escalado, pantalla completa, teclado, ratón y portapapeles.
4. Integrar la VDM de forma transparente con cmd y el Explorador, con redirecciones y ERRORLEVEL.
5. Dejar operativo el DOS de modo texto: COMMAND.COM integrado, ficheros, XMS, EMS, ratón INT 33h, altavoz y reposo sin gastar CPU.
6. Montar un arnés de pruebas propio en QEMU/KVM y medir el rendimiento de Fast486 como puerta para la Fase 2.

### 1.3 Contexto

- **Qué hizo LiberNT.** El 30/04/2026 dejó de compilar NTVDM con Clang (`4c2171c`). El 12/10/2026 lo borró entero (`054ce27`) al pasar la consola a `condrv.sys` + `conhost`, porque el servidor de consola que usaba ya no existía.
- **Qué sigue en el árbol.** Fast486 (CPU 386/486 en C, GPL-2.0-or-later, ya compila en amd64 porque la HAL la usa para llamadas BIOS), las rutas VDM de kernel32 (`CreateProcessInternalW`, `vdm.c`) y de basesrv para DOS, y la clasificación de ejecutables DOS y NE en NVS.
- **Qué falta.** `ntvdm.exe` y su frontend. Además, NTVDM ponía el PC emulado en la dirección 0 y kernel32 reservaba ahí 16 MB; NVS lo prohíbe, igual que Windows 8 en adelante.
- **Excepción a una regla de upstream.** `AGENTS.md` toma Windows 11 como referencia, pero Windows 11 no tiene NTVDM. Para esta feature la referencia es la documentación de MS-DOS, el comportamiento de DOSBox y los ficheros de referencia del corpus de pruebas (ADR-06).
- **Fuera de alcance de esta fase:** validar modos gráficos, Sound Blaster, OPL3, MIDI, velocidad por programa, disquetes, ajustes por programa (.PIF), interpretar `CONFIG.NT`/`AUTOEXEC.NT`, DPMI y Win16. Todo está en el roadmap del maestro.

---

## 2. Arquitectura Técnica

### 2.1 Stack tecnológico

| Capa | Tecnología | Notas |
|------|------------|-------|
| Sistema base | LiberNT `main-nt10`, fork en GitHub | Fusión semanal de upstream |
| Lenguaje | C, estilo LiberNT/ReactOS (`CODING_STYLE.md`) | Sin comentarios en los diffs (`AGENTS.md`) |
| Build | CMake + Ninja, `configure.sh`, rosconfig (menuconfig) | Clang por defecto; GCC en i386 y amd64 |
| Toolchain | RosBE (`rosbe-unix-bootstrap.sh`) | llvm-mingw y mingw-gcc |
| CPU emulada | Fast486 (`sdk/lib/fast486`) | 386/486 con FPU, GPL-2.0-or-later |
| VDM | NTVDM de ReactOS (`subsystems/mvdm`) | BIOS y DOS emulados en código del host |
| Ventana | Win32 (user32, gdi32) | Pintado con `StretchDIBits` |
| Integración | kernel32 + basesrv (CSR) | Rutas VDM ya existentes |
| Altavoz | `beep.sys` (`\Device\Beep`) | Ya existe en LiberNT |
| Código de 16 bits | Ensamblador integrado de Clang (`asm16`); C con `clang -m16` | COMMAND.COM y programas de prueba |
| Pruebas | Python 3 (solo biblioteca estándar), QEMU/KVM + QMP, mtools | En el Linux del desarrollador (Omarchy) |
| CI | GitHub Actions (`build.yml` de upstream) | 7 trabajos por push |

### 2.2 Estructura de carpetas

```
librent/                                   fork de ahmedarif193/LiberNT, rama main
├── AGENTS.md                              upstream, sin cambios
├── CLAUDE.md                              NUEVO (fork)
├── LIBRENT_VERSION                        NUEVO: 0.1.0 al cerrar la fase
├── NOVEDADES.md                           NUEVO: novedades visibles por versión
├── docs/                                  NUEVO (fork)
│   ├── librent-maestro.md
│   ├── librent-fase1-informe.md
│   ├── librent-fase1-checklist.md
│   ├── librent-fase1-prompt.md
│   ├── librent-fase1-implementacion.md    lo escribe el agente
│   └── usuario/dos.md                     documentación de usuario
├── subsystems/
│   ├── CMakeLists.txt                     upstream: añade mvdm tras ENABLE_NTVDM
│   └── mvdm/                              RESTAURADO desde 054ce27^
│       ├── CMakeLists.txt, asm16.cmake
│       ├── config/                        CONFIG.NT, AUTOEXEC.NT, system.ini, win.ini
│       ├── dos/                           command.S → command.com
│       ├── pifmgr/                        iconos de programas DOS
│       ├── vdmdbg/                        ayuda de depuración (de Wine)
│       └── ntvdm/
│           ├── bios/ cpu/ dos/ hardware/  heredados (adaptados a 64 bits)
│           ├── frontend/                  NUEVO: ventana, pintado, entrada, portapapeles, ajustes
│           ├── testhooks.c                NUEVO: NTVDM_STATS, NTVDM_SCREEN_DUMP
│           ├── lang/ res/                 heredados (+ textos nuevos en en-US y es-ES)
│           └── console/                   ELIMINADO (lo sustituye frontend/)
├── dll/win32/kernel32/client/vdm.c        upstream: BaseGetVdmConfigInfo sin reserva baja
├── sdk/cmake/rosconfig.def                upstream: ENABLE_NTVDM, ENABLE_NTVDM_BOOT_RUN
├── sdk/cmake/config.cmake                 upstream: espejo de las dos opciones
├── sdk/lib/fast486/                       upstream: sin cambios salvo arreglos justificados
├── modules/rostests/tests/ntvdm16/        NUEVO: programas de prueba de 16 bits y vmtestmark.exe
└── scripts/librent/                       NUEVO: vmtest.py, corpus/manifest.json, corpus/golden/
```

`wow16` y `samples` no se restauran: Win16 llega en la Fase 4 y `testvdd` solo compilaba en i386.

### 2.3 Ficheros de upstream que se tocan

Upstream sigue avanzando unos 170 commits al día. Para que la fusión semanal sea barata, solo se tocan estos ficheros fuera de las rutas del fork. Cualquier otro cambio en un fichero de upstream se anota como desviación en el informe de implementación.

| Fichero | Cambio |
|---------|--------|
| `subsystems/CMakeLists.txt` | `add_subdirectory(mvdm)` cuando `ENABLE_NTVDM` está activa, sin condición de compilador |
| `sdk/cmake/rosconfig.def` | Opciones `ENABLE_NTVDM` (menú System) y `ENABLE_NTVDM_BOOT_RUN` (menú Testing) |
| `sdk/cmake/config.cmake` | Espejo de las dos opciones |
| `dll/win32/kernel32/client/vdm.c` | `BaseGetVdmConfigInfo`: tamaño de reserva 0 y error claro si falta `ntvdm.exe` |
| Fichero que activa las opciones `*_BOOT_RUN` existentes (p. ej. `ENABLE_CPUBENCH_BOOT_RUN`) | Entrada de arranque de `ENABLE_NTVDM_BOOT_RUN` |

### 2.4 Diagrama de arquitectura

```
                         Usuario
              ┌─────────────┴──────────────┐
          cmd.exe                       Explorador
              │ CreateProcess              │ ShellExecuteEx → CreateProcess
              └─────────────┬──────────────┘
                            ▼
   kernel32.dll   CreateProcessInternalW: NtCreateSection devuelve
                  STATUS_INVALID_IMAGE_NOT_MZ / _PROTECT / _WIN_16
                            │ BaseCheckVDM (CSR)          ▲ código de salida
                            ▼                             │ (GetExitCodeProcess)
   csrss.exe / basesrv   VDM_CONSOLE_RECORD ─< VDM_DOS_RECORD
                         concede VdmAllowed al proceso nuevo
                            │ crea "ntvdm.exe -i<sesión>"  ▲ GetNextVDMCommand / ExitVDM
                            ▼                              │
 ┌───────────────────────────── ntvdm.exe (proceso de usuario) ────────────────────────────┐
 │ Hilo de ventana (frontend/)           │ Hilo de emulación                                │
 │ ┌───────────────────────────────┐     │ ┌──────────────────────────────────────────────┐ │
 │ │ VDM_FRONTEND                  │pinta│ │ Fast486 · reloj y temporizadores             │ │
 │ │ ventana, pintado texto/gráfico│◄────┤ │ PIC · PIT · PS/2 · VGA (svga.c) · altavoz    │ │
 │ │ teclado, ratón, portapapeles  ├────►│ │ BIOS (HLE) · DOS32 (dem, XMS, EMS, ratón)    │ │
 │ └───────────────────────────────┘entra│ │ COMMAND.COM integrado                        │ │
 │                                       │ └─────────────────────┬────────────────────────┘ │
 │ testhooks.c: NTVDM_STATS,             │   VDM_GUEST_MEMORY: 16 MB en cualquier dirección │
 │ NTVDM_SCREEN_DUMP                     │   + páginas de guarda                            │
 └────────────────┬──────────────────────┴──────────────────────┬───────────────────────────┘
                  │ Win32: ficheros (NTFS/FAT), registro,         │ \Device\Beep
                  │ portapapeles                                  │ (altavoz)
```

---

## 3. Modelo de Datos

Este proyecto no tiene base de datos. El modelo son estructuras en memoria, claves de registro y ficheros. Los nombres de entidad siguen la convención de tipos de LiberNT (`MAYUSCULAS_SNAKE_CASE`). Los campos de estructuras C y valores de registro van en `CamelCase` como en upstream; los de ficheros de texto y JSON, en `minusculas_snake_case`.

### 3.1 Diagrama E-R

```
   VDM_CONSOLE_RECORD (basesrv) 1 ──────────< N VDM_DOS_RECORD (basesrv)
            │ 1                                    │ N
            │ atiende (una VDM viva por registro)  │ se ejecuta en
            ▼ 1                                    ▼ 1
     ntvdm.exe (proceso VDM) ◄────────────────────┘
      │1            │1                 │1                  │0..1             │0..1
      ▼1            ▼1                 ▼1                  ▼                 ▼
 VDM_GUEST_MEMORY  VDM_FRONTEND ──1───< N CHAR_CELL     NTVDM_STATS    NTVDM_SCREEN_DUMP
                    │ N lee                              (fichero)      (ficheros)
                    ▼ 1
               VDM_SETTINGS (HKCU)

   NTVDM_MACHINE_CONFIG (HKLM) 1 ──< N ntvdm.exe   (lo lee cada VDM al arrancar)

   CORPUS_ITEM 1 ──────────────< N TEST_RESULT     (uno por ejecución del arnés)
        │ produce, en la VM                           │ incluye
        └──► NTVDM_STATS / NTVDM_SCREEN_DUMP ─────────┘
```

### 3.2 Definición de estructuras

#### VDM_CONSOLE_RECORD (existente, `subsystems/win/basesrv/vdm.h`)

Registro de basesrv que agrupa las tareas DOS de una consola, o de una sesión DOS sin consola.

| Campo | Tipo | Descripción |
|-------|------|-------------|
| `ConsoleHandle` | HANDLE | Consola del proceso que lanza (NULL si no tiene) |
| `ProcessHandle` | HANDLE | Proceso `ntvdm.exe` que lo atiende |
| `ServerEvent` / `ClientEvent` | HANDLE | Par de eventos para esperar la siguiente orden |
| `ProcessId` | ULONG | PID de la VDM |
| `ReenterCount` | ULONG | Reentradas DOS → Win32 → DOS |
| `CurDirsLength` | ULONG | Longitud de los directorios actuales guardados |
| `SessionId` | ULONG | ID de sesión DOS cuando no hay consola (se pasa como `-i<hex>`) |
| `DosListHead` | LIST_ENTRY | Lista de `VDM_DOS_RECORD` |

#### VDM_DOS_RECORD (existente, `subsystems/win/basesrv/vdm.h`)

Una tarea DOS pedida por un proceso Win32. Tiene ciclo de vida (§3.4).

| Campo | Tipo | Descripción |
|-------|------|-------------|
| `State` | USHORT | `VDM_NOT_READY` (pendiente o en ejecución) o `VDM_READY` (terminada) |
| `ExitCode` | ULONG | Código de salida del programa DOS |
| `ServerEvent` / `ClientEvent` | HANDLE | Aviso al proceso que espera |
| `CommandInfo` | PVDM_COMMAND_INFO | Orden pendiente; NULL cuando la VDM ya la ha recogido |

#### VDM_GUEST_MEMORY (existente como globales de `memory.c` y `emulator.c`)

La memoria del PC emulado. No hace falta una estructura nueva: se documenta su contrato.

| Campo | Tipo | Descripción |
|-------|------|-------------|
| `BaseAddress` | PVOID | Base del bloque en el proceso. **Nunca 0 en librent** |
| `Size` | SIZE_T | `MAX_ADDRESS` = 16 MB (`0x1000000`) |
| `GuardSize` | SIZE_T | NUEVO: 64 KB sin acceso detrás del bloque |
| `PageTable` | PMEM_HOOK[] | Ganchos de memoria de los VDD (existente) |

Mapa del invitado: `00000h–9FFFFh` memoria convencional, `A0000h–BFFFFh` vídeo, `C0000h–FFFFFh` ROM y UMB, `100000h–10FFEFh` HMA, `110000h–FFFFFFh` XMS.

#### VDM_FRONTEND (nueva, `subsystems/mvdm/ntvdm/frontend/frontend.h`)

Estado de la ventana de la VDM. Tiene ciclo de vida (§3.4).

| Campo | Tipo | Descripción |
|-------|------|-------------|
| `Window` | HWND | Ventana de la VDM |
| `UiThread` | HANDLE | Hilo con el bucle de mensajes |
| `State` | enum | `Creada`, `Visible`, `Cerrando`, `Destruida` |
| `Presentation` | enum | `Ventana` o `PantallaCompleta` |
| `Mode` | enum | `Texto` o `Grafico` |
| `TextResolution` | COORD | Columnas × filas (40/80 × 25/43/50) |
| `CellWidth` / `CellHeight` | ULONG | 8 o 9 puntos de ancho; 8, 14 o 16 de alto |
| `TextFramebuffer` | PCHAR_CELL | Búfer de texto que escribe `svga.c` |
| `GraphicsFramebuffer` | PBYTE | Búfer gráfico indexado (sin validar en F1) |
| `Palette` | RGBQUAD[256] | Paleta DAC actual |
| `CursorVisible`, `CursorStart`, `CursorEnd`, `CursorLocation` | BOOL, BYTE, BYTE, WORD | Cursor de texto del CRTC |
| `Scale` | ULONG | 0 automática; 1–4 fija |
| `DosMouseCursorVisible` | BOOL | El programa muestra su cursor (se oculta el del sistema) |
| `PasteQueue` | cola de BYTE | Bytes CP437 pendientes de pegar |
| `ProgramName` | WCHAR[MAX_PATH] | Para el título |

#### CHAR_CELL (existente, `subsystems/mvdm/ntvdm/hardware/video/svga.h`)

| Campo | Tipo | Descripción |
|-------|------|-------------|
| `Char` | CHAR | Carácter en CP437 |
| `Attributes` | BYTE | Color de texto (bits 0–3), fondo (4–6), parpadeo o fondo intenso (7) |

#### VDM_SETTINGS (nueva, `HKCU\Software\librent\NTVDM`)

| Valor | Tipo | Defecto | Descripción |
|-------|------|---------|-------------|
| `WindowScale` | REG_DWORD | 0 | 0 automática; 1–4 fija |
| `StartFullScreen` | REG_DWORD | 0 | 1 = la VDM arranca en pantalla completa |
| `WindowX`, `WindowY` | REG_DWORD | sin valor | Última posición de la ventana |

#### NTVDM_MACHINE_CONFIG (existente, `HKLM\SYSTEM\CurrentControlSet\Control\NTVDM`)

| Valor | Tipo | Descripción |
|-------|------|-------------|
| `BiosFile` | REG_SZ | BIOS alternativa (vacío: BIOS integrada) |
| `RomFiles` | REG_MULTI_SZ | ROM opcionales |
| `FloppyDisk0`, `FloppyDisk1` | REG_SZ | Imágenes de disquete |
| `HardDisk0` … `HardDisk3` | REG_SZ | Imágenes de disco duro |

Se mantiene tal cual. En F1 no tiene interfaz; montar disquetes desde la ventana es de la Fase 2.

#### NTVDM_STATS (nuevo, fichero `clave=valor` en la ruta de la variable `NTVDM_STATS`)

| Clave | Tipo | Descripción |
|-------|------|-------------|
| `pid` | entero | PID de la VDM |
| `program` | texto | Programa DOS ejecutado |
| `exit_code` | entero | Código de salida |
| `startup_ms` | entero | Desde el inicio del proceso hasta la primera instrucción del programa |
| `wall_ms` | entero | Tiempo de reloj total |
| `cpu_ms` | entero | Tiempo de CPU del proceso (usuario + kernel) |
| `idle_ms` | entero | Tiempo con la CPU emulada en reposo |
| `instructions` | entero | Instrucciones emuladas |
| `mips_avg` | decimal | Millones de instrucciones por segundo fuera del reposo |
| `speaker_events` | entero | Cambios de estado del altavoz |
| `last_video_mode` | entero | Último modo de vídeo fijado con INT 10h |
| `error` | texto | Aviso que habría mostrado un diálogo (con `NTVDM_NO_DIALOGS=1`) |

#### NTVDM_SCREEN_DUMP (nuevo, ficheros en la ruta de la variable `NTVDM_SCREEN_DUMP`)

| Fichero | Contenido |
|---------|-----------|
| `<ruta>` | Pantalla de texto en UTF-8 (CP437 convertido), una línea por fila, sin espacios finales |
| `<ruta>.attr` | Atributos en hexadecimal, dos dígitos por celda, una línea por fila |
| `<ruta>.<ms>` y `<ruta>.<ms>.attr` | Volcado adicional pedido con `NTVDM_SCREEN_DUMP_MS` |

#### CORPUS_ITEM (nuevo, `scripts/librent/corpus/manifest.json`)

Tiene ciclo de vida (§3.4).

| Campo | Tipo | Descripción |
|-------|------|-------------|
| `id` | texto | Identificador único (`freedos-mem`, `own-xmstest`…) |
| `name` | texto | Nombre legible |
| `source` | texto | `repo` (prueba propia) o `url` (descarga) |
| `url` | texto | Origen de la descarga (si `source` es `url`) |
| `sha256` | texto | Huella del fichero descargado |
| `license` | texto | Licencia (identificador SPDX o `shareware`) |
| `files` | lista | Ficheros que se copian al disco de pruebas |
| `command` | texto | Orden que ejecuta `RUNTESTS.CMD` (con redirecciones si hacen falta) |
| `keys` | lista | Secuencia de teclas QMP y espera inicial en ms (opcional) |
| `expect` | objeto | `exit_code`, `stdout_golden`, `screen_golden`, `stats` (umbrales) |
| `category` | texto | `texto`, `ficheros`, `xms`, `ems`, `raton`, `altavoz`, `reposo`, `rendimiento`, `integracion` |

#### TEST_RESULT (nuevo, `RESULTS\` del disco de pruebas y salida de `vmtest.py report`)

Tiene ciclo de vida (§3.4).

| Campo | Tipo | Descripción |
|-------|------|-------------|
| `item_id` | texto | `CORPUS_ITEM.id` |
| `status` | texto | `pendiente`, `ejecutando`, `ok`, `fallo`, `tiempo_agotado` |
| `exit_code` | entero | Código de salida obtenido |
| `diffs` | lista | Diferencias con los ficheros de referencia |
| `stats` | objeto | Copia de `NTVDM_STATS` |
| `duration_ms` | entero | Duración de la prueba |

### 3.3 Relaciones

- Un `VDM_CONSOLE_RECORD` agrupa N `VDM_DOS_RECORD`. Con la VDM efímera (ADR-05), N suele ser 1; es mayor si llegan órdenes mientras la VDM vive.
- Cada `ntvdm.exe` tiene exactamente una `VDM_GUEST_MEMORY` y un `VDM_FRONTEND`. El frontend pinta columnas × filas `CHAR_CELL`.
- `VDM_SETTINGS` es por usuario y `NTVDM_MACHINE_CONFIG` por máquina; la VDM los lee al arrancar.
- `NTVDM_STATS` y `NTVDM_SCREEN_DUMP` solo existen si se definen sus variables de entorno.
- Cada `CORPUS_ITEM` produce un `TEST_RESULT` por ejecución del arnés.

### 3.4 Diagramas de estados

**VDM_DOS_RECORD (basesrv)**

```
 BaseSrvCheckVDM                     GetNextVDMCommand
 ───────────────► PENDIENTE ───────────────────────────► EN_EJECUCION
                 (VDM_NOT_READY,      la VDM recoge       (VDM_NOT_READY,
                  CommandInfo ≠ NULL) la orden             CommandInfo = NULL)
                      │                                         │ GetNextVDMCommand con
                      │ fallo al arrancar la VDM                │ código de salida (fin de tarea)
                      │ (kernel32 deshace el registro)          ▼
                      └────────────► ELIMINADO ◄──────── TERMINADA (VDM_READY, ExitCode)
                                                 ExitVDM / GetVDMExitCode
 Transiciones inválidas:
   TERMINADA → EN_EJECUCION   (una tarea terminada no se reanuda)
   EN_EJECUCION → PENDIENTE   (la orden ya se entregó)
   PENDIENTE → TERMINADA      (sin pasar por EN_EJECUCION)
```

**Ciclo de vida de la VDM (proceso `ntvdm.exe`, reflejado en `VDM_CONSOLE_RECORD`)**

```
                 memoria, BIOS, VGA, DOS
 ARRANCANDO ───────── y ventana listos ─────────► EJECUTANDO ◄───────────► EN_REPOSO
     │                                              │        HLT, INT 16h en espera,
     │ fallo: memoria, sin VdmAllowed,              │        INT 28h, INT 2Fh 1680h
     │ -w/-ws (Windows 16 bits)                     │        ⇄ IRQ, tecla o temporizador
     │                                              │
     │                                              │ no quedan órdenes de su consola,
     │                                              │ cierre confirmado, "Terminar la VDM"
     │                                              │ o excepción del invitado
     ▼                                              ▼
  CERRADA ◄─────────────────────────────────── TERMINANDO
 Transiciones inválidas:
   CERRADA → cualquier estado    (la VDM no se reutiliza ni queda residente)
   ARRANCANDO → EN_REPOSO        (no hay programa todavía)
   TERMINANDO → EJECUTANDO       (no se cancela una salida en curso)
```

**VDM_FRONTEND**

```
 Ciclo:         CREADA ──► VISIBLE ──(cerrar)──► CERRANDO ──(confirmado o sin programa)──► DESTRUIDA
                              ▲                      │
                              └────(cancelar)────────┘
 Presentación:  VENTANA ⇄ PANTALLA_COMPLETA        (Alt+Intro, menú, StartFullScreen)
 Contenido:     MODO_TEXTO ⇄ MODO_GRAFICO          (cambio de modo de vídeo; gráfico sin validar en F1)
 Transiciones inválidas:
   DESTRUIDA → VISIBLE           (se crea una ventana nueva, nunca se resucita)
   CREADA → CERRANDO             (no se puede cerrar lo que no se ha mostrado)
```

**CORPUS_ITEM (arnés)**

```
 DECLARADO ──descarga──► DESCARGADO ──SHA-256 correcto──► VERIFICADO ──► copiado al disco de pruebas
                              │
                              └──SHA-256 distinto──► RECHAZADO (vmtest.py payload falla)
 Transiciones inválidas:
   RECHAZADO → VERIFICADO        (hay que descargar de nuevo y volver a verificar)
   DECLARADO → VERIFICADO        (sin descarga; salvo source = repo, que se verifica en la build)
```

**TEST_RESULT (arnés)**

```
 PENDIENTE ──RUNTESTS.CMD lo lanza──► EJECUTANDO ──┬──► OK
                                                   ├──► FALLO           (código, salida o umbral distintos)
                                                   └──► TIEMPO_AGOTADO  (sin marca de fin a tiempo)
 Transiciones inválidas:
   OK, FALLO o TIEMPO_AGOTADO → cualquier otro (los resultados de una ejecución no se reescriben)
   PENDIENTE → OK                (sin ejecutar)
```

---

## 4. Flujos de Trabajo

### 4.1 Lanzar un programa DOS desde cmd

```
 cmd: C:\JUEGOS> PROG.EXE /x
   │
   ▼
 kernel32: NtCreateSection ──► STATUS_INVALID_IMAGE_PROTECT (MZ de DOS)
   │                            STATUS_INVALID_IMAGE_NOT_MZ  (.COM)
   │                            STATUS_INVALID_IMAGE_WIN_16  (NE de Windows 3.x)
   ▼
 BaseCheckVDM ──► basesrv ¿VDMDisallowed? ── sí ──► ERROR_VDM_DISALLOWED ──► fin
   │ no
   ▼
 basesrv crea VDM_CONSOLE_RECORD + VDM_DOS_RECORD (PENDIENTE) → VDM_NOT_LOADED
   │
   ▼
 BaseGetVdmConfigInfo ¿existe system32\ntvdm.exe? ── no ──► deshace registros
   │ sí (reserva = 0, nada en la dirección 0)            └──► ERROR_BAD_EXE_FORMAT ──► fin
   ▼
 CreateProcess("ntvdm.exe -i<sesión> [-w]") ; basesrv concede VdmAllowed
   │
   ▼
 ntvdm ¿VdmAllowed? ── no ──► sale con 0 sin hacer nada ──► fin
   │ sí
   ├─ ¿-w o -ws? ── sí ──► aviso «Windows 16 bits aún no» ──► salida 193 ──► fin
   ▼
 Reserva 16 MB + 64 KB de guarda en cualquier dirección ── fallo ──► aviso ──► salida con error ──► fin
   ▼
 BIOS, VGA, PS/2, DOS32, COMMAND.COM; hilo de ventana; la ventana aparece
   ▼
 GetNextVDMCommand ──► DOS_RECORD EN_EJECUCION ──► COMMAND.COM ejecuta PROG.EXE
   ▼
 Programa en marcha (EJECUTANDO ⇄ EN_REPOSO) ── excepción ──► aviso con CS:IP ──► TERMINANDO
   ▼ INT 21h 4Ch
 GetNextVDMCommand(código) ──► DOS_RECORD TERMINADA
   ▼
 ¿quedan órdenes de esta consola? ── sí ──► las ejecuta en la misma ventana
   │ no
   ▼
 ExitVDM ──► la ventana se cierra ──► ntvdm sale con el código del programa
   ▼
 cmd: ERRORLEVEL = código; el foco vuelve a la consola
```

**Paso a paso**

1. cmd llama a `CreateProcess`. El kernel no puede crear una sección de imagen y devuelve el estado que identifica el formato.
2. kernel32 pregunta a basesrv con `BaseCheckVDM`. Si la directiva `VDMDisallowed` está activa, falla con `ERROR_VDM_DISALLOWED`.
3. basesrv registra la consola y la tarea y responde `VDM_NOT_LOADED`: no hay VDM viva para esa consola.
4. `BaseGetVdmConfigInfo` compone `ntvdm.exe -i<sesión>` con reserva 0. Si `ntvdm.exe` no existe (`ENABLE_NTVDM` desactivada), devuelve `ERROR_BAD_EXE_FORMAT` y kernel32 deshace los registros, como haría Windows de 64 bits.
5. kernel32 crea `ntvdm.exe`; basesrv le concede `VdmAllowed` (necesita `SeTcbPrivilege`, que solo tiene CSRSS).
6. La VDM comprueba `VdmAllowed`, reserva su memoria en cualquier dirección y arranca la BIOS, la VGA, el DOS y la ventana.
7. COMMAND.COM recoge la orden con `GetNextVDMCommand` y ejecuta el programa.
8. Al terminar, la VDM informa del código; basesrv marca la tarea como terminada y avisa a quien espera.
9. Si no quedan órdenes para esa consola, la VDM llama a `ExitVDM` y sale con el código del programa. cmd lo recibe como ERRORLEVEL.

### 4.2 Doble clic en el Explorador

```
 Explorador: doble clic en PROG.EXE
   ▼
 ShellExecuteEx ──► CreateProcess (el Explorador no tiene consola)
   ▼
 Mismo camino que 4.1 (basesrv usa un SessionId porque no hay consola)
   ▼
 ntvdm (CUI) recibe una consola nueva
   ▼
 ¿es el único proceso de esa consola? ── sí ──► oculta la ventana de la consola
   ▼
 Ventana de la VDM visible ──► programa ──► fin ──► la ventana se cierra
   │
   └─ errores: igual que 4.1, pero los avisos van en diálogo (no hay consola visible)
```

### 4.3 Entrada de teclado y ratón

```
 WM_KEYDOWN / WM_KEYUP (hilo de ventana)
   ▼
 ¿atajo reservado (Alt+Intro)? ── sí ──► alterna pantalla completa (no llega al DOS)
   ▼ no
 KEY_EVENT_RECORD {código de exploración del lParam, E0 si es tecla extendida, estado}
   ▼
 KeyboardEventHandler ──► cola PS/2 ──► IRQ 1 ──► BIOS INT 09h
   ▼
 MapVirtualKey + ToUnicode (distribución activa de Windows) ──► CP437
   │                                  └─ sin equivalente en CP437: solo código de exploración
   ▼
 Búfer de teclado de la BIOS ──► INT 16h / INT 21h ──► programa

 WM_MOUSEMOVE / WM_xBUTTONxxx
   ▼
 posición en el área cliente ──► celda ──► coordenadas virtuales DOS
   ▼
 MouseEventHandler ──► controlador INT 33h (mouse32.c) ──► programa
   └─ si el programa muestra su cursor: se oculta el del sistema sobre la ventana
```

### 4.4 Pegar texto

```
 Menú «Pegar» ──► portapapeles CF_UNICODETEXT
   ▼
 Unicode ──► CP437 (caracteres sin equivalente se omiten; \r\n → Intro)
   ▼
 PasteQueue ──► cada vez que el búfer de la BIOS tiene hueco, se empujan teclas
   └─ si el programa lee el teclado por puertos (sin BIOS), no recibe lo pegado (limitación documentada)
```

### 4.5 Ciclo de pruebas automáticas

```
 Host (Linux del desarrollador)
   │ vmtest.py payload: lee manifest.json ──► descarga a caché ──► SHA-256
   │        └─ SHA-256 distinto ──► CORPUS_ITEM RECHAZADO ──► la orden falla ──► fin
   │ crea disco FAT32 (mtools): \LIBRENT\RUNTESTS.CMD, programas, RESULTS\
   ▼
 vmtest.py run: QEMU/KVM sin pantalla, -snapshot, serie a fichero, QMP, disco de pruebas
   ▼
 LiberNT arranca ──► sesión ──► entrada Run de ENABLE_NTVDM_BOOT_RUN
   ▼
 librent-vmtest.cmd busca \LIBRENT\RUNTESTS.CMD de C: a Z: ── no está ──► sale sin hacer nada
   ▼
 Por cada prueba: fija NTVDM_STATS, NTVDM_SCREEN_DUMP, NTVDM_NO_DIALOGS=1
   │   ¿tiene teclas? ── sí ──► vmtestmark "LIBRENT-VMTEST-KEYS <id>" ──► vmtest.py las envía por QMP
   │   ejecuta la orden (con redirecciones) y anota el código en RESULTS\
   ▼
 vmtestmark "LIBRENT-VMTEST-DONE" (salida de depuración ──► serie) ──► shutdown /s /t 0
   ▼
 vmtest.py: ¿marca a tiempo? ── no ──► mata QEMU ──► pendientes = TIEMPO_AGOTADO
   ▼
 mtools extrae RESULTS\ ──► vmtest.py report compara con golden/ ──► tabla + código de salida
```

### 4.6 Fusión semanal de upstream

```
 git fetch upstream
   ▼
 rama sync/AAAAMMDD desde main ──► git merge upstream/main-nt10
   ▼
 ¿conflictos? ── sí ──► fuera de las rutas del fork manda upstream;
   │                    en los ficheros de §2.3 se reaplica el cambio mínimo
   ▼
 compila amd64, i386 y arm64 ──► vmtest.py all en amd64
   ▼
 ¿todo verde? ── no ──► se arregla en la rama (nunca se reescribe historia publicada)
   ▼ sí
 PR a main del fork (nunca a ahmedarif193/LiberNT)
```

### 4.7 Diagramas de secuencia

**Lanzamiento y código de salida**

```
 cmd.exe        kernel32               basesrv (CSRSS)            ntvdm.exe
    │ CreateProcess │                         │                         │
    │──────────────►│ NtCreateSection         │                         │
    │               │ → INVALID_IMAGE_PROTECT │                         │
    │               │ BaseCheckVDM ──────────►│ crea registros          │
    │               │◄──────── VDM_NOT_LOADED │ (DOS_RECORD PENDIENTE)  │
    │               │ BaseGetVdmConfigInfo    │                         │
    │               │ (reserva 0, existe exe) │                         │
    │               │ CreateProcess(ntvdm) ──►│ VdmAllowed ────────────►│ arranca
    │◄─ handle VDM ─│                         │                         │
    │ WaitForSingleObject                     │◄── GetNextVDMCommand ───│
    │               │                         │ orden ─────────────────►│ COMMAND.COM
    │               │                         │ (EN_EJECUCION)          │ ejecuta PROG
    │               │                         │◄─ GetNextVDMCommand(c) ─│ fin del programa
    │               │                         │ (TERMINADA)             │
    │               │                         │◄──────── ExitVDM ───────│
    │◄───────────────────────────── el proceso termina con el código c ─│
    │ GetExitCodeProcess → BaseCheckForVDM → ERRORLEVEL = c             │
```

**Pulsación de tecla**

```
 Usuario   Hilo ventana        PS/2/PIC           Fast486 + BIOS          Programa DOS
   │ tecla    │                    │                     │                      │
   │─────────►│ WM_KEYDOWN         │                     │                      │
   │          │ KeyboardEventHandler►│ cola, IRQ 1 ──────►│ INT 09h:             │
   │          │                    │                     │ ToUnicode → CP437    │
   │          │                    │                     │ búfer BIOS           │
   │          │                    │                     │◄──── INT 16h AH=00h ─│
   │          │                    │                     │ (si estaba en reposo,│
   │          │                    │                     │  despierta)          │
   │          │                    │                     │──── AX = scan/char ─►│
```

**Ejecución del arnés**

```
 vmtest.py        QEMU/KVM         LiberNT (RUNTESTS.CMD)      ntvdm.exe          mtools
   │ arranca ────►│ boot ──────────►│ inicio de sesión            │                   │
   │              │                 │ por cada prueba ───────────►│ ejecuta, escribe  │
   │              │                 │                             │ NTVDM_STATS y     │
   │◄─ serie: LIBRENT-VMTEST-KEYS ──│                             │ volcados          │
   │ QMP send-key ───────────────────────────────────────────────►│                   │
   │              │                 │◄──────────── código ────────│                   │
   │◄─ serie: LIBRENT-VMTEST-DONE ──│ shutdown                    │                   │
   │◄─ QEMU termina                 │                             │                   │
   │ extrae RESULTS\ ─────────────────────────────────────────────────────────────────►│
   │ report: compara con golden/                                                      │
```

---

## 5. Interfaces

Este proyecto no expone endpoints HTTP. Esta sección recoge las interfaces que la fase crea, cambia o verifica. El checklist las repite en su sección «Interfaces» con los mismos identificadores.

### 5.1 Frontend (sustituye a `console/video.h`)

Se conserva la interfaz para no tocar `svga.c`; cambia su implementación.

| Interfaz | Llamada desde | Comportamiento en librent |
|----------|---------------|---------------------------|
| `VgaConsoleInitialize` | `VgaInitialize` (svga.c) | Crea el hilo de ventana y la ventana (aún oculta) |
| `VgaConsoleCleanup` | `VgaCleanup` | Cierra la ventana y espera al hilo |
| `VgaConsoleCreateTextScreen` | svga.c al fijar un modo de texto | Prepara el búfer `CHAR_CELL` y la rejilla de celdas |
| `VgaConsoleDestroyTextScreen` | svga.c | Libera la rejilla (el búfer de texto nunca se pone a NULL) |
| `VgaConsoleCreateGraphicsScreen` | svga.c al fijar un modo gráfico | DIB indexado con paleta (implementado, sin validar en F1) |
| `VgaConsoleDestroyGraphicsScreen` | svga.c | Libera el DIB |
| `VgaConsoleRepaintScreen` | svga.c | Invalida el rectángulo afectado; pinta el hilo de ventana |
| `VgaConsoleUpdateTextCursor` | svga.c | Forma, posición y visibilidad del cursor |
| `VgaAttachToConsole` | dem.c, al volver de un programa Win32 | Vuelve a ocultar la consola si estaba oculta y reactiva la entrada |
| `VgaDetachFromConsole` | dem.c, antes de un programa Win32 | Si la consola estaba oculta, la muestra para ese programa |
| `VgaGetDoubleVisionState` | svga.c | Sin cambios |
| `ScreenEventHandler` | — | Se elimina: ya no hay eventos de tamaño de consola |

### 5.2 Entrada

| Interfaz | Llamada desde | Comportamiento en librent |
|----------|---------------|---------------------------|
| `KeyboardEventHandler` | Hilo de ventana | Recibe un `KEY_EVENT_RECORD` construido desde `WM_KEYDOWN`/`WM_KEYUP` |
| `MouseEventHandler` | Hilo de ventana | Recibe un `MOUSE_EVENT_RECORD` con la posición en celdas |
| `FocusEventHandler` | Hilo de ventana | `WM_SETFOCUS`/`WM_KILLFOCUS` |

### 5.3 Integración con Win32 y CSR

| Interfaz | Dónde | Cambio |
|----------|-------|--------|
| `CreateProcessInternalW` | kernel32 | Ninguno; se verifica la ruta VDM |
| `BaseCheckVDM` | kernel32 → `BaseSrvCheckVDM` | Ninguno |
| `BaseGetVdmConfigInfo` | kernel32 | Reserva 0 (nada en la dirección 0) y `ERROR_BAD_EXE_FORMAT` si falta `ntvdm.exe` |
| `GetNextVDMCommand` | ntvdm (dem.c) → `BaseSrvGetNextVDMCommand` | VDM efímera: antes de salir consulta si quedan órdenes |
| `ExitVDM` | ntvdm → `BaseSrvExitVDM` | Se llama siempre antes de salir |
| `BaseCheckForVDM` | kernel32 (`GetExitCodeProcess`) | Ninguno; se verifica el código de salida |

### 5.4 Servicios del PC emulado que se validan en F1

| Servicio | Funciones | Prueba |
|----------|-----------|--------|
| INT 10h | 00h (modos 00h–03h y 07h), 01h–03h, 05h–0Ah, 0Eh, 0Fh, 11h (1110h, 1111h, 1112h, 1114h), 13h | `textmodes`, `hello` |
| INT 16h | 00h–02h, 10h–12h | `keytest`, `idle` |
| INT 21h | 02h, 06h–0Ah, 0Eh, 19h, 1Ah, 25h, 2Ah, 2Ch, 2Fh, 30h, 35h, 39h–43h, 47h, 48h–4Fh, 56h | `hello`, `stdio`, `filetest`, `memtest`, `exitcode` |
| INT 2Fh | 4300h, 4310h (XMS), 1680h (liberar tiempo) | `xmstest`, `idle` |
| XMS (llamada lejana) | 00h–0Bh | `xmstest` |
| INT 67h | 40h–48h | `emstest` |
| INT 33h | 00h–04h, 07h, 08h | `mousetest` |
| INT 1Ah | 00h, 02h, 04h | `filetest` (fecha y hora) |
| Puertos | 42h, 43h, 61h (PIT canal 2 y altavoz) | `speaker` |

### 5.5 Configuración y build

| Interfaz | Dónde | Comportamiento |
|----------|-------|----------------|
| `ENABLE_NTVDM` | `rosconfig.def` (System) y `config.cmake` | Booleana `auto`: activa en i386, amd64 y arm64; desactivada en riscv64 y ppc |
| `ENABLE_NTVDM_BOOT_RUN` | `rosconfig.def` (Testing, solo Debug) | Instala `librent-vmtest.cmd`, `vmtestmark.exe` y la entrada `Run` del arnés |
| `HKCU\Software\librent\NTVDM` | Registro | `VDM_SETTINGS` |
| `HKLM\SYSTEM\CurrentControlSet\Control\NTVDM` | Registro | `NTVDM_MACHINE_CONFIG`, sin cambios |
| `VDMDisallowed` | `HKLM\Software\Policies\Microsoft\Windows\AppCompat` | Directiva existente que bloquea la VDM |

### 5.6 Ganchos de prueba (variables de entorno)

Se leen una vez al arrancar la VDM. Sin ellas, la VDM se comporta igual que en uso normal.

| Interfaz | Valor | Comportamiento |
|----------|-------|----------------|
| `NTVDM_STATS` | Ruta | Escribe `NTVDM_STATS` al terminar |
| `NTVDM_SCREEN_DUMP` | Ruta | Vuelca la pantalla de texto al terminar |
| `NTVDM_SCREEN_DUMP_MS` | Milisegundos | Vuelco adicional a ese tiempo desde el inicio del programa |
| `NTVDM_NO_DIALOGS` | `1` | Sin diálogos: los avisos van a stderr y a `NTVDM_STATS`; cerrar no pide confirmación |

### 5.7 Arnés (`scripts/librent/vmtest.py`)

| Interfaz | Parámetros | Comportamiento |
|----------|------------|----------------|
| `vmtest.py payload` | `--manifest`, `--build-dir`, `--cache`, `--out` | Descarga y verifica el corpus, toma las pruebas propias de la build y crea el disco FAT32 |
| `vmtest.py run` | `--image`, `--payload`, `--timeout`, `--results`, `--serial-log` | QEMU/KVM sin pantalla con `-snapshot`; inyecta teclas por QMP; espera la marca de fin |
| `vmtest.py report` | `--results`, `--golden` | Compara con las referencias, imprime una tabla y sale con error si algo falla |
| `vmtest.py all` | Los de las tres anteriores | Encadena `payload`, `run` y `report` |

---

## 6. Interfaz de Usuario

### 6.1 Mapa de navegación

```
Inicio
├── cmd.exe ──── PROG.EXE ──────────────────┐
├── Explorador ── doble clic en PROG.EXE ───┤
│                                           ▼
│                         Ventana «PROG — VDM de librent»
│                         ├── Contenido DOS (texto 40/80 × 25/43/50)
│                         ├── Alt+Intro ──► Pantalla completa ⇄ Ventana
│                         ├── Menú del sistema (Alt+Espacio o clic en el icono)
│                         │   ├── Pantalla completa        Alt+Intro
│                         │   ├── Escala ▸ Automática · 1× · 2× · 3× · 4×
│                         │   ├── Copiar texto
│                         │   ├── Pegar
│                         │   ├── Volcar memoria ▸ Texto · Binario
│                         │   ├── Terminar la VDM
│                         │   └── Acerca de la VDM… ──► diálogo con versiones
│                         └── Cerrar (X) ──► confirmación si el programa sigue en marcha
└── Avisos
    ├── Aplicación de Windows de 16 bits (Fase 4)
    ├── Excepción del programa DOS (CS:IP)
    └── Sin memoria para la VDM
```

### 6.2 Mockups

**Ventana de la VDM con EDIT de FreeDOS (escala 2×)**

```
┌─ EDIT — VDM de librent ─────────────────────────────────────────── _ □ X ┐
│  File  Edit  Search  View  Options  Help                                 │
│ ┌─────────────────────────────── NOTAS.TXT ──────────────────────────────┐│
│ │Lista de la compra                                                      ││
│ │- pan                                                                   ││
│ │- leche                                                                 ││
│ │- café_                                                                 ││
│ │                                                                        ││
│ │                                                                        ││
│ └────────────────────────────────────────────────────────────────────────┘│
│ F1=Help                                                  Line:4  Col:7    │
└───────────────────────────────────────────────────────────────────────────┘
```

**Menú del sistema**

```
┌─ EDIT — VDM de librent ────────────── _ □ X ┐
│┌──────────────────────────────┐             │
││ Restaurar                    │             │
││ Mover                        │             │
││ Minimizar                    │             │
│├──────────────────────────────┤             │
││ Pantalla completa  Alt+Intro │             │
││ Escala                     ▸ │ Automática ✓│
││ Copiar texto                 │ 1×          │
││ Pegar                        │ 2×          │
││ Volcar memoria             ▸ │ 3×          │
││ Terminar la VDM              │ 4×          │
│├──────────────────────────────┤             │
││ Acerca de la VDM…            │             │
││ Cerrar               Alt+F4  │             │
│└──────────────────────────────┘             │
└─────────────────────────────────────────────┘
```

**Acerca de la VDM**

```
┌─ Acerca de la VDM ─────────────────────────┐
│  [icono DOS]  VDM de librent               │
│               librent 0.1.0                │
│               LiberNT 0.4.17 (dev)         │
│               CPU emulada: Fast486 (486)   │
│               Memoria: 16 MB · XMS · EMS   │
│                                            │
│  Licencia GPL-3.0-or-later; código         │
│  heredado de ReactOS GPL-2.0-or-later.     │
│                               [ Aceptar ]  │
└────────────────────────────────────────────┘
```

**Confirmación al cerrar**

```
┌─ VDM de librent ─────────────────────────────┐
│  (!)  EDIT sigue en marcha.                  │
│       Si cierras la ventana, se perderán     │
│       los cambios que no haya guardado.      │
│                                              │
│              [ Terminar ]  [ Cancelar ]      │
└──────────────────────────────────────────────┘
```

**Sesión de cmd con redirección, ERRORLEVEL y aviso de Win16**

```
C:\PRUEBAS> type datos.txt | stdio.com > salida.txt
C:\PRUEBAS> type salida.txt
HOLA DESDE DOS
C:\PRUEBAS> exitcode.com 7
C:\PRUEBAS> echo %errorlevel%
7
C:\PRUEBAS> CALC16.EXE
Las aplicaciones de Windows de 16 bits aún no son compatibles con librent.
C:\PRUEBAS> echo %errorlevel%
193
```

### 6.3 Notas de UX y diseño

- Píxeles nítidos: escalado entero sin suavizado. «Automática» usa el mayor factor que cabe, hasta 4×.
- Pantalla completa sin cambiar el modo de vídeo del sistema: ventana sin bordes del tamaño del monitor, imagen centrada y bandas negras.
- El título lleva el nombre del programa DOS.
- El icono es el de programa DOS de `ntvdm` y `pifmgr.dll`.
- Al terminar el programa, el foco vuelve a la ventana anterior (cmd o el Explorador).
- Los textos nuevos van en `en-US` y `es-ES`; el resto de idiomas recibe la cadena inglesa para no romper los recursos.
- Sin colores ni fuentes propias dentro del área DOS: se pinta lo que manda la VGA emulada.

---

## 7. Lógica de Negocio

### 7.1 Memoria reubicable y validación

La VDM reserva y confirma 16 MB más 64 KB de guarda en la dirección que elija el sistema. Todas las conversiones de dirección del invitado a puntero del host pasan por una función que comprueba el rango. Fast486 ya accede a memoria por callbacks con direcciones de 32 bits, así que la CPU emulada no nota la diferencia.

```python
GUEST_SIZE = 0x1000000   # MAX_ADDRESS: 16 MB

def linear(seg: int, off: int, a20_enabled: bool) -> int:
    addr = (seg << 4) + off
    return addr if a20_enabled else addr & 0xFFFFF      # sin A20, envuelve en 1 MB

def to_host(base: int, guest_linear: int, size: int) -> int:
    if guest_linear < 0 or size < 0 or guest_linear + size > GUEST_SIZE:
        raise ValueError("acceso fuera de la memoria del invitado")
    return base + guest_linear
```

Reglas:
- `BaseAddress` nunca es 0. Si la reserva falla, la VDM avisa y sale con error.
- `BaseGetVdmConfigInfo` devuelve un tamaño de reserva 0, así que kernel32 no reserva nada en el proceso hijo.
- Un acceso fuera de rango desde un servicio (BOP, BIOS o DOS) se rechaza como error del invitado, nunca como acceso a memoria del host.

### 7.2 Pintado del modo texto

- La fuente sale del plano 2 de la VGA emulada. Así se respetan las fuentes que carga el programa (INT 10h 1110h) y las de 8×8, 8×14 y 8×16 de la BIOS.
- Ancho de celda de 8 o 9 puntos según el registro de reloj del secuenciador. Alto según el registro de altura de carácter del CRTC.
- Con 9 puntos y gráficos de línea activos, la novena columna repite la octava en los caracteres `C0h–DFh`, como la VGA real.
- Atributo: color de texto en bits 0–3, fondo en 4–6, bit 7 como parpadeo o fondo intenso según el registro de modo del controlador de atributos.
- Cursor: forma (líneas inicial y final), posición y visibilidad del CRTC; parpadeo a unos 3,75 Hz.

```python
def decode_attr(attr: int, blink_enabled: bool):
    fg = attr & 0x0F
    bg = (attr >> 4) & (0x07 if blink_enabled else 0x0F)
    blink = blink_enabled and bool(attr & 0x80)
    return fg, bg, blink

def ninth_column_on(char: int, glyph_row: int, line_graphics: bool) -> bool:
    return line_graphics and 0xC0 <= char <= 0xDF and bool(glyph_row & 0x01)
```

### 7.3 Escalado

```python
def auto_scale(native_w: int, native_h: int, avail_w: int, avail_h: int, max_scale: int = 4) -> int:
    n = min(avail_w // native_w, avail_h // native_h, max_scale)
    return max(1, n)

# 80×25 con celdas de 9×16 = 720×400; en un área libre de 1920×1040 → 2× (1440×800)
assert auto_scale(720, 400, 1920, 1040) == 2
```

### 7.4 Teclado y página de códigos OEM 437

- El código de exploración sale del `lParam` de `WM_KEYDOWN`/`WM_KEYUP` (bits 16–23) y la tecla extendida, del bit 24 (prefijo `E0`).
- El carácter se calcula con la distribución activa de Windows y se convierte a CP437, que es la página de la fuente VGA. Si no tiene equivalente, se envía solo el código de exploración.
- La misma página se usa para nombres de fichero (la VDM llama a `SetFileApisToOEM` o convierte de forma explícita) y para el portapapeles.

```python
def to_cp437(ch: str) -> int | None:
    try:
        return ch.encode("cp437")[0]
    except UnicodeEncodeError:
        return None          # sin equivalente: no hay carácter, solo código de exploración

assert to_cp437("ñ") == 0xA4 and to_cp437("Ñ") == 0xA5 and to_cp437("€") is None
```

### 7.5 Ratón (INT 33h con coordenadas absolutas)

El puntero del sistema se traduce a la celda bajo él y de ahí a las coordenadas virtuales del controlador de ratón. No hay captura del ratón en F1; la captura relativa, para juegos, es de la Fase 2.

```python
def mouse_virtual(px: int, py: int, client_w: int, client_h: int, cols: int, rows: int):
    col = min(cols - 1, px * cols // client_w)
    row = min(rows - 1, py * rows // client_h)
    return col * (640 // cols), row * 8      # CX y DX de INT 33h AX=3 en modo texto

assert mouse_virtual(10 * 18 + 3, 5 * 32 + 3, 1440, 800, 80, 25) == (80, 40)
```

### 7.6 Reposo de la CPU emulada

Cuando el programa espera, la CPU emulada se detiene hasta la siguiente interrupción, tecla o temporizador. Se activa con:
- `HLT`;
- INT 16h de lectura bloqueante con el búfer vacío;
- INT 28h (DOS en espera);
- INT 2Fh AX=1680h (liberar el resto del tiempo).

Los temporizadores del PIT siguen corriendo con el reloj real, así que la hora DOS no se retrasa.

```python
IDLE_TRIGGERS = {"HLT", "INT16_BLOCKING_EMPTY", "INT28", "INT2F_1680"}

def next_action(trigger: str, irq_pending: bool) -> str:
    if trigger in IDLE_TRIGGERS and not irq_pending:
        return "esperar a IRQ, tecla o temporizador"
    return "seguir emulando"
```

### 7.7 Redirección, códigos de salida y VDM efímera

- Si la entrada o la salida estándar del proceso que lanza no son la consola (fichero o tubería), los handles DOS 0, 1 y 2 se enlazan con ellas. Lo que el programa escribe con INT 21h va al destino redirigido. Lo que escribe directamente en la memoria de vídeo o con la BIOS solo se ve en la ventana.
- El código de INT 21h 4Ch llega a cmd como ERRORLEVEL. Si el usuario cierra la ventana y confirma, el código es `STATUS_CONTROL_C_EXIT`.
- Un Ctrl+C pulsado en la consola de cmd mientras corre el programa llega al DOS como Ctrl+Pausa.
- VDM efímera: al terminar un programa, la VDM pregunta a basesrv si quedan órdenes de su consola. Si quedan, las ejecuta en la misma ventana; si no, llama a `ExitVDM` y termina. Nunca queda residente.
- Programas DOS que lanzan otros DOS (INT 21h 4Bh) se ejecutan en la misma VDM. Si lanzan un programa Win32, la VDM espera a que termine.

### 7.8 Rendimiento y umbrales (puerta de la Fase 2)

`bench16` ejecuta una carga fija de enteros, memoria, cadenas y FPU. `NTVDM_STATS` da las instrucciones emuladas y los tiempos. Los umbrales son orientativos, a partir de cifras habituales de MIPS de Intel (386DX a 33 MHz ≈ 11 MIPS; 486DX a 33 MHz ≈ 27 MIPS).

```python
def mips(instructions: int, wall_ms: int, idle_ms: int) -> float:
    active_s = max(1e-9, (wall_ms - idle_ms) / 1000)
    return instructions / active_s / 1e6

def clasificar(m: float) -> str:
    if m < 10:
        return "insuficiente: la Fase 2 empieza con un sprint de optimización (D-01)"
    if m < 25:
        return "suficiente para juegos de 1990–1992 (≈386DX a 33 MHz)"
    return "suficiente para la mayoría de juegos de modo real (≈486DX a 33 MHz o más)"
```

### 7.9 Reglas de validación y casos especiales

| Caso | Comportamiento |
|------|----------------|
| Ejecutable NE (Windows 3.x) | La VDM recibe `-w` o `-ws`, avisa de que Win16 aún no está y sale con 193 (`ERROR_BAD_EXE_FORMAT`) sin ejecutar el stub de DOS |
| `ENABLE_NTVDM` desactivada | `CreateProcess` falla con `ERROR_BAD_EXE_FORMAT`; no quedan registros en basesrv |
| Directiva `VDMDisallowed` | `CreateProcess` falla con `ERROR_VDM_DISALLOWED` |
| Fichero sin nombre 8.3 | El DOS no lo ve. En F1 se comprueba si el NTFS de LiberNT da nombres cortos; los alias virtuales quedan como decisión abierta (D-02) |
| Ruta con caracteres fuera de CP437 | No se puede representar en DOS; se documenta como limitación |
| Ejecutar `COMMAND.COM` a mano | Comportamiento `ntcmdprompt` de NT: abre la VDM y lanza cmd.exe. No hay intérprete DOS interactivo propio en F1 |
| `.PIF` | Fuera de alcance (ajustes por programa en la Fase 2) |
| Programa que lee el teclado por puertos | No recibe el texto pegado (sí las pulsaciones reales) |
| Excepción del invitado sin manejador | Aviso con CS:IP y opción de terminar; con `NTVDM_NO_DIALOGS=1`, sale con error y lo anota en `NTVDM_STATS` |
| Dos programas DOS a la vez desde consolas distintas | Dos VDM independientes, cada una con su ventana |
| arm64 | Compila; la validación funcional es de amd64 en esta fase |

---

## 8. Requisitos No Funcionales

### 8.1 Seguridad

- La VDM no obtiene privilegios nuevos. `VdmAllowed` solo lo concede basesrv, que es quien tiene `SeTcbPrivilege`.
- Memoria del invitado confinada: conversiones validadas y 64 KB de guarda. Un programa DOS no puede leer ni escribir memoria del host fuera de su bloque.
- Los ficheros se abren con los permisos del usuario que lanza el programa.
- Se respeta la directiva `VDMDisallowed`.
- Los ganchos de prueba solo leen variables de entorno y escriben en las rutas que indican.
- Sin red, sin servicios ni puertos abiertos. Autenticación, roles y rate limiting no aplican.

### 8.2 Protección de datos (RGPD)

No aplica: la fase no trata datos personales. Los registros de depuración no incluyen el contenido de los ficheros del usuario. El corpus de pruebas solo contiene software publicado por sus autores.

### 8.3 Backups y recuperación

No aplica al producto. El código vive en git (GitHub) con CI. El arnés arranca la imagen con `-snapshot` y nunca la modifica. Un fallo de la VDM se queda en su proceso: no afecta al sistema ni a otras VDM.

### 8.4 Rendimiento

| Métrica | Objetivo | Cómo se mide |
|---------|----------|--------------|
| Arranque de la VDM | `startup_ms` < 2000 en QEMU/KVM, amd64 Debug | `NTVDM_STATS` de `hello` |
| Reposo | `cpu_ms` < 10 % de `wall_ms` con EDIT esperando 10 s | `NTVDM_STATS` de la prueba `idle` |
| Rendimiento de la CPU emulada | Se mide y se informa; decide D-01 | `bench16` + `NTVDM_STATS` |
| Memoria | Bloque del invitado de 16 MB; conjunto de trabajo del proceso < 64 MB | Administrador de tareas (prueba manual) |

### 8.5 Accesibilidad

- Todo se puede manejar con el teclado: Alt+Espacio abre el menú del sistema, Alt+Intro alterna pantalla completa y Alt+F4 cierra.
- Escalado hasta 4× para leer mejor.
- Textos de interfaz localizados (`en-US` y `es-ES`).
- La VDM no añade parpadeos propios; los que haya son del programa DOS.

### 8.6 Compatibilidad, licencias y mantenimiento

- Compila en i386, amd64 y arm64 con el compilador de cada trabajo del CI; sin `#ifdef` de arquitectura nuevos.
- El código heredado conserva GPL-2.0-or-later y sus cabeceras. Los ficheros nuevos llevan la cabecera de `AGENTS.md` con GPL-3.0-or-later.
- Prohibido incorporar código filtrado de Windows (NTVDMx64 incluido) o con licencia GPL-2.0-only.
- Los cambios en ficheros de upstream se limitan a los de §2.3.

---

## 9. Integraciones

### 9.1 Internas del sistema

| Componente | Uso |
|------------|-----|
| kernel32 + basesrv (CSR) | Lanzamiento transparente, órdenes, códigos de salida |
| `beep.sys` | Tonos del altavoz |
| user32 / gdi32 | Ventana, pintado, menús, diálogos |
| Portapapeles | Copiar y pegar texto |
| Registro | `VDM_SETTINGS`, `NTVDM_MACHINE_CONFIG`, `VDMDisallowed` |

### 9.2 Externas (solo desarrollo)

| Herramienta | Uso |
|-------------|-----|
| QEMU/KVM + QMP | Arranque sin pantalla, inyección de teclas |
| mtools | Crear el disco de pruebas FAT32 y extraer resultados |
| Paquetes de FreeDOS (GPL) | Corpus externo: EDIT, MEM, FIND, MORE, SORT, CHOICE, TREE |
| GitHub (`gh`, Actions) | Fork, PR dentro del fork, CI |

### 9.3 Formatos de fichero

| Formato | Tratamiento |
|---------|-------------|
| `.COM` | Imagen plana cargada en el desplazamiento 100h |
| `.EXE` MZ de DOS | Cargado por el DOS32 de la VDM |
| `.EXE` NE (Windows 3.x) | Detectado y rechazado con aviso |
| `CONFIG.NT`, `AUTOEXEC.NT` | Se instalan en `system32`; se interpretan en la Fase 2 |
| `manifest.json` | Corpus de pruebas (`CORPUS_ITEM`) |
| `NTVDM_STATS`, `NTVDM_SCREEN_DUMP` | Salidas de los ganchos de prueba |
| Imagen FAT32 | Disco de pruebas (D: o la letra que asigne el sistema) |

### 9.4 Autenticación

No aplica. `gh` usa la sesión del desarrollador.

---

## 10. Decisiones Técnicas (ADR-lite)

**ADR-01 · Partir del NTVDM borrado y de Fast486.** Se restaura `subsystems/mvdm` desde `054ce27^`. Descartadas: un núcleo tipo DOSBox Staging (habría que rehacer la integración con kernel32 y basesrv) y escribirlo desde cero (meses sin valor visible). NTVDMx64 queda prohibido porque procede del código filtrado de NT4. Motivo: el código ya encaja con CSR, su licencia es compatible y Fast486 ya compila en amd64.

**ADR-02 · Memoria del invitado reubicable.** El bloque de 16 MB va en cualquier dirección. Descartada: una excepción de página nula en NVS para procesos VDM. Motivo: no tocar el kernel, mantener la semántica NT10 de upstream y evitar conflictos en la zona más activa de LiberNT. NTVDM ya tenía las macros (`REAL_TO_PHYS`, modo `STANDALONE`).

**ADR-03 · Ventana propia para texto y gráficos.** Descartadas: texto en la consola de cmd con ventana aparte para gráficos, y devolver el soporte VDM a conhost. Motivo: las APIs privadas de consola que usaba NTVDM ya no existen, conhost es código vivo de upstream y la ventana da fidelidad VGA real.

**ADR-04 · Integración transparente con `ENABLE_NTVDM` en `auto`.** Descartadas: lanzador explícito y opción desactivada por defecto. Motivo: es la promesa de la feature (programas de los 90 sin máquina virtual) y las rutas de kernel32 y basesrv ya existen.

**ADR-05 · VDM efímera por consola.** La VDM ejecuta las órdenes de su consola mientras vive y termina al quedarse sin ellas. Descartadas: VDM residente por consola como en NT, y una VDM por programa sin cola. Motivo: con condrv no está claro que basesrv empareje la VDM con su consola por `ConsoleHandle` (R-02); así no quedan procesos residentes y la ventana tiene un ciclo claro.

**ADR-06 · Referencia de comportamiento.** Documentación de MS-DOS, DOSBox y ficheros de referencia del corpus. Descartada: Windows 11 (no tiene NTVDM) y Windows 10 de 32 bits en VM (opción 12b no elegida). Motivo: es una excepción explícita a la regla de paridad de `AGENTS.md`.

**ADR-07 · Convenciones de upstream en código y commits.** Inglés, `[COMPONENTE]` con Problem/Why/How y sin comentarios; castellano en documentos y respuestas. Descartadas: las reglas de la casa en todo. Motivo: menos conflictos al fusionar y posibilidad de proponer cambios a upstream.

**ADR-08 · Arnés propio.** `vmtest.py` con QEMU/KVM, QMP, mtools y la opción `ENABLE_NTVDM_BOOT_RUN`. Descartados: el `vm_monitor.py` de upstream (no es público) y solo pruebas manuales. Motivo: `AGENTS.md` exige ver cada arreglo funcionar en un arranque real.

**ADR-09 · Página de códigos OEM 437 coherente.** Teclado, nombres de fichero y portapapeles en CP437. Descartadas: seguir con ANSI como el código heredado (rompe ñ y tildes) y CP850 (la fuente VGA de la BIOS es CP437). Motivo: coherencia con la fuente y con DOS.

**ADR-10 · Reposo de la CPU emulada.** Descartado: el bucle activo heredado, que gasta un núcleo entero aunque el programa espere. Motivo: un editor DOS abierto no puede tener la CPU al 100 %.

**ADR-11 · Código de 16 bits con Clang.** `asm16` con el ensamblador integrado y pruebas en C con `-m16`. Descartado: depender de `i686-w64-mingw32-as`, que no está en todos los RosBE (el de macOS no trae GCC). Motivo: build reproducible con el compilador por defecto.

**ADR-12 · Versión propia del fork.** `LIBRENT_VERSION` y `NOVEDADES.md` en la raíz. Descartado: editar `sdk/include/reactos/version.cmake`. Motivo: evitar conflictos con cada subida de versión de upstream.

**ADR-13 · Win16 en F1: aviso y salida 193 desde la VDM.** Descartados: dejar que corra el stub de DOS («This program requires Microsoft Windows») y rechazarlo en kernel32. Motivo: mensaje claro sin tocar kernel32; la Fase 4 reutilizará la misma entrada `-w`.

---

## 11. Plan de Desarrollo

Sin estimación en semanas (decisión del proyecto): sprints con tamaño relativo.

| Sprint | Objetivo | Tareas principales | Tamaño |
|--------|----------|--------------------|--------|
| S1 | Fork y compilación | Fork y remotos; workflows `stale`/`labeler` desactivados; `CLAUDE.md`, `docs/`, `LIBRENT_VERSION`, `NOVEDADES.md`; restaurar `subsystems/mvdm`; `ENABLE_NTVDM`; `asm16` con Clang; compilar i386, amd64 y arm64 | L |
| S2 | 64 bits y primer arranque | Memoria reubicable y guarda; `BaseGetVdmConfigInfo`; validación de punteros; arnés mínimo (`run` + `report`) y `ENABLE_NTVDM_BOOT_RUN`; `hello.com` con salida redirigida en QEMU | M |
| S3 | Ventana y entrada | Frontend (hilo, ventana, texto con plano 2, celdas, cursor, escalado, pantalla completa, menú, ajustes, acerca de); teclado CP437; ratón; copiar y pegar; ocultar consola sobrante | L |
| S4 | Integración y DOS | cmd y Explorador; redirecciones; ERRORLEVEL; VDM efímera; Win16 y errores; ficheros y nombres 8.3; XMS; EMS; altavoz; reposo; excepciones | M |
| S5 | Corpus, rendimiento y cierre | Pruebas propias completas; corpus FreeDOS con SHA-256; referencias; QMP; `bench16` y medición; primera fusión de upstream; documentación de usuario; versión 0.1.0 | L |

**Supuestos:**
- Claude Code trabaja en el Linux del desarrollador (Omarchy) con RosBE, QEMU con KVM, mtools y Python 3.
- La primera build completa de cada arquitectura es larga; luego se compila de forma incremental. `CCACHE_DISABLE=1`, como pide `AGENTS.md`.
- Las pruebas automáticas se ejecutan en local; el CI de GitHub solo compila.
- Las fusiones semanales de upstream pueden interrumpir un sprint si hay conflictos.

---

## 12. Conexiones con Otras Fases

### 12.1 Dependencias

| Fase | Qué necesita de esta |
|------|----------------------|
| F2 · Gráficos, sonido y velocidad | Superficie gráfica del frontend, temporizadores, arnés y corpus, `NTVDM_STATS`, medición de rendimiento (D-01) |
| F3 · Extensores DOS (DPMI) | Memoria reubicable ampliable más allá de 16 MB, validación de punteros, XMS |
| F4 · Windows 3.x (Win16) | Entrada `-w`/`-ws` de la VDM, rutas WOW de basesrv (hoy `UNIMPLEMENTED`) |
| Upstream | Rutas VDM de kernel32 y basesrv: si upstream las borrara, el fork las conserva (D-05) |

### 12.2 Datos compartidos

- `VDM_SETTINGS` crecerá con los ajustes de la Fase 2 (escala gráfica, velocidad, captura del ratón).
- `CORPUS_ITEM` y `TEST_RESULT` sirven para todas las fases; el manifiesto suma juegos en la Fase 2.
- `NTVDM_STATS` añadirá métricas de vídeo y sonido en la Fase 2.
