# librent - Informe Maestro

**Última actualización:** 17/10/2026

## 1. Visión Global

librent es un fork propio de LiberNT, el sistema operativo libre de arquitectura NT derivado de ReactOS que mantiene Ahmed ARIF (`github.com/ahmedarif193/LiberNT`). LiberNT apunta a compatibilidad con Windows 11, se centra en 64 bits (amd64, arm64 y riscv64) y avanza muy deprisa, con mucho apoyo de IA. librent toma esa base, la sincroniza cada semana y añade funciones que upstream no tiene o ha retirado.

La primera es devolver la ejecución de programas DOS y de 16 bits en sistemas de 64 bits. LiberNT eliminó NTVDM el 12/10/2026 al cambiar su consola, y Microsoft documenta que Windows de 64 bits no ejecuta código x86 de 16 bits. librent quiere que los programas y juegos de los 90 funcionen con doble clic, sin máquina virtual.

Cliente y contexto: proyecto personal de Hector. Licencia GPL-3.0 (lo heredado de ReactOS conserva la suya). Lo desarrolla Claude Code siguiendo esta metodología por fases, con pruebas en QEMU/KVM en el Linux del desarrollador.

## 2. Mapa de Módulos

```
 ┌──────────────────────────────────────────────┐
 │ LiberNT main-nt10 (upstream)                 │
 └──────────────────────┬───────────────────────┘
                        │ fusión semanal (T-05)
                        ▼
 ┌──────────────────────────────────────────────┐
 │ Base del fork (dentro de F1): repo, CI,      │
 │ arnés vmtest.py, versión y novedades         │
 └──────────────────────┬───────────────────────┘
                        ▼
 ┌───────────────────────────┐    ┌───────────────────────────┐    ┌───────────────────────────┐
 │ F1 NTVDM en 64 bits       │───►│ F2 Gráficos, sonido y     │───►│ F3 Extensores DOS en modo │
 │ (DOS en modo texto)       │    │ velocidad (juegos)        │    │ protegido (DPMI)          │
 │ Especificada              │    │ Propuesta · puerta D-01   │    │ Propuesta                 │
 └─────────────┬─────────────┘    └───────────────────────────┘    └─────────────┬─────────────┘
               │                                                                 │ orden elegido
               │ dependencia técnica                                             ▼
               │                                                   ┌───────────────────────────┐
               └──────────────────────────────────────────────────►│ F4 Windows 3.x (Win16)    │
                                                                   │ Propuesta                 │
                                                                   └───────────────────────────┘
```

## 3. Tabla Resumen de Fases

| Fase | Módulo | Funcionalidades | Sprints | Semanas | Estado |
|------|--------|-----------------|---------|---------|--------|
| 1 | NTVDM en 64 bits (DOS en modo texto) | 122 | 5 | — | Especificada |
| 2 | Gráficos, sonido y velocidad (juegos) | — | — | — | Propuesta |
| 3 | Extensores DOS en modo protegido (DPMI) | — | — | — | Propuesta |
| 4 | Windows 3.x (Win16) | — | — | — | Propuesta |
| **Total** | | **122** | **5** | **—** | |

Sin estimación en semanas por decisión del proyecto (T-09): cada informe de fase da el tamaño relativo de sus sprints.

## 4. Modelo de Datos Global

```
   VDM_CONSOLE_RECORD [F1, existente] 1 ────────< N VDM_DOS_RECORD [F1, existente]
            │ 1                                         │ N
            ▼ 1                                         ▼ 1
     ntvdm.exe (proceso VDM) ◄─────────────────────────┘
      │1              │1                         │0..1                  │0..1
      ▼1              ▼1                         ▼                      ▼
 VDM_GUEST_MEMORY  VDM_FRONTEND [F1] ──1──< N CHAR_CELL      NTVDM_STATS [F1]  NTVDM_SCREEN_DUMP [F1]
 [F1, documentada]    │ N lee                [F1, existente]
                      ▼ 1
               VDM_SETTINGS [F1] (HKCU\Software\librent\NTVDM)

   NTVDM_MACHINE_CONFIG [F1, existente] (HKLM\SYSTEM\CurrentControlSet\Control\NTVDM) 1 ──< N ntvdm.exe

   CORPUS_ITEM [F1] 1 ──────────< N TEST_RESULT [F1]
```

Las fases 2 a 4 ampliarán `VDM_SETTINGS` (ajustes por programa), `NTVDM_STATS` (vídeo y sonido), `CORPUS_ITEM` (juegos) y `VDM_GUEST_MEMORY` (más de 16 MB en la F3).

## 5. Glosario de Dominio

| Término de negocio | Entidad/Tabla | Fase | Notas |
|--------------------|---------------|------|-------|
| VDM (máquina DOS virtual) | `VDM_CONSOLE_RECORD` | 1 | Proceso `ntvdm.exe`; basesrv la registra por consola o sesión |
| Tarea DOS | `VDM_DOS_RECORD` | 1 | Pendiente → en ejecución → terminada |
| Memoria del invitado | `VDM_GUEST_MEMORY` | 1 | 16 MB reubicables y 64 KB de guarda |
| Ventana de la VDM | `VDM_FRONTEND` | 1 | Sustituye al frontend de consola |
| Celda de texto | `CHAR_CELL` | 1 | Carácter CP437 + atributo |
| Ajustes de la ventana | `VDM_SETTINGS` | 1 | `HKCU\Software\librent\NTVDM` |
| Configuración de la máquina | `NTVDM_MACHINE_CONFIG` | 1 | BIOS, ROM y discos (`HKLM`) |
| Estadísticas de la VDM | `NTVDM_STATS` | 1 | Fichero `clave=valor` |
| Volcado de pantalla | `NTVDM_SCREEN_DUMP` | 1 | Texto UTF-8 + atributos |
| Elemento del corpus | `CORPUS_ITEM` | 1 | `manifest.json` |
| Resultado de prueba | `TEST_RESULT` | 1 | Salida del arnés |
| VDM efímera | — | 1 | Vive mientras tenga órdenes de su consola; nunca residente |
| NTVDM | — | 1 | Implementación de ReactOS restaurada en `subsystems/mvdm` |
| Fast486 | — | 1 | CPU 386/486 emulada (`sdk/lib/fast486`) |
| BOP | — | 1 | Instrucción `C4 C4` con la que el código de 16 bits llama al host |
| COMMAND.COM | — | 1 | Intérprete integrado (`dos/command.S`); no es interactivo en F1 |
| XMS / HMA | — | 1 | Memoria extendida; HMA en 100000h–10FFEFh |
| EMS | — | 1 | Memoria expandida (LIM 4.0) |
| ERRORLEVEL | — | 1 | Código de salida que recibe cmd |
| Página de códigos OEM 437 | — | 1 | La de la fuente VGA; teclado, ficheros y portapapeles |
| Upstream | — | 1 | `ahmedarif193/LiberNT`, rama `main-nt10` |
| Zonas del fork | — | 1 | Rutas propias de librent (ver T-05) |
| DPMI | — | 3 | Interfaz de modo protegido para extensores DOS |
| WOW | — | 4 | Windows on Windows: ejecución de Win16 |

## 6. Estado del Proyecto

| Fase | Progreso | Último informe de implementación procesado | Estado |
|------|----------|---------------------------------------------|--------|
| 1 | 0 / 122 (sin confirmar) | — | Especificada |

**Progreso total acumulado:** 0 / 122 funcionalidades.

## 7. Roadmap

### F2 · Gráficos, sonido y velocidad (juegos)
- Modos gráficos CGA, EGA, VGA y Modo X validados en la ventana, con corrección de aspecto 4:3.
- Sound Blaster 16 (DSP y DMA), OPL3 con Nuked-OPL3 (LGPL-2.1) y MPU-401 hacia el MIDI del sistema.
- Velocidad ajustable por programa (ciclos), captura relativa del ratón, montar imágenes de disquete desde la ventana.
- Ajustes por programa (.PIF o propiedades) e interpretación de `AUTOEXEC.NT` (`SET BLASTER`) y `CONFIG.NT`.
- Depende de: F1 cerrada y la decisión D-01 sobre el rendimiento de Fast486.

### F3 · Extensores DOS en modo protegido
- DOS/4GW, PMODE/W y DOS32A: modo raw/XMS, VCPI o host DPMI 0.9 propio.
- Memoria del invitado por encima de 16 MB, INT 15h (87h, 88h, E801h), A20 por el puerto 92h y el 8042.
- Pruebas con juegos shareware de 1993 en adelante.
- Depende de: F2 (orden elegido); técnicamente, de F1.

### F4 · Windows 3.x (Win16)
- Rutas WOW de basesrv (hoy `UNIMPLEMENTED`), cargador NE y llamadas Win16 → Win32 sobre Fast486, con Wine (LGPL) como base.
- Revisar licencias de winevdm/otvdm antes de reutilizar nada (D-04).
- Depende de: F1; va la última por orden elegido.

### Candidatas sin fase
- Intérprete DOS interactivo (FreeCOM u otro).
- Página de códigos 850 y fuentes asociadas.
- Alias 8.3 virtuales si el NTFS de LiberNT no da nombres cortos (D-02).
- Recompilador dinámico (JIT) para Fast486, según D-01.
- Validación funcional en arm64 sobre una Raspberry Pi real.

## 8. Decisiones Transversales y Riesgos Abiertos

### 8.1 Decisiones transversales

| ID | Decisión | Origen |
|----|----------|--------|
| T-01 | Código y commits con las convenciones de upstream (`AGENTS.md`): inglés, `[COMPONENTE]` con Problem/Why/How o `feat:`, sin comentarios en los diffs. Documentos y respuestas del agente en castellano | Descubrimiento 3a |
| T-02 | Clean-room y fuentes de verdad de `AGENTS.md`. Prohibido código filtrado de Windows (incluido NTVDMx64) y código GPL-2.0-only | Descubrimiento 7a |
| T-03 | Neutralidad de arquitectura: sin `#ifdef` de arquitectura nuevos; compilar i386, amd64 y arm64 antes de dar algo por terminado | Descubrimiento 6a |
| T-04 | La VDM no cambia el núcleo: nada de excepciones en NVS; memoria del invitado reubicable | Descubrimiento 8a |
| T-05 | Fusión semanal de `upstream/main-nt10`. Zonas del fork: `subsystems/mvdm`, `modules/rostests/tests/ntvdm16`, `scripts/librent`, `docs`, `CLAUDE.md`, `LIBRENT_VERSION`, `NOVEDADES.md`. Fuera de ellas, solo los ficheros que liste cada informe de fase | Descubrimiento 2a |
| T-06 | Excepción a la paridad con Windows 11 para DOS y 16 bits: referencia en la documentación de MS-DOS, DOSBox y ficheros de referencia | Fase 1, ADR-06 |
| T-07 | Arnés propio `scripts/librent/vmtest.py` (QEMU/KVM, QMP, mtools) y corpus fuera del repo, verificado por SHA-256 | Descubrimiento 12a (supuesto) |
| T-08 | Regla permanente: cada funcionalidad nueva lleva documentación de usuario en `docs/usuario/`, subida de `LIBRENT_VERSION` y entrada en `NOVEDADES.md` | Regla de la casa |
| T-09 | Sin estimación en semanas: sprints con tamaño relativo | Descubrimiento 12a (supuesto) |
| T-10 | Commits sin firma ni `Co-Authored-By`, solo cuando el desarrollador lo pide; las PR van siempre contra el fork, nunca contra upstream | Regla de la casa + `AGENTS.md` |

### 8.2 Decisiones abiertas

| ID | Decisión | Cuándo |
|----|----------|--------|
| D-01 | Si los MIPS de Fast486 (§7.8 del informe F1) bastan para la F2 o hace falta un sprint de optimización o un JIT | Al procesar el informe de implementación de la F1 |
| D-02 | Alias 8.3 virtuales si el NTFS de LiberNT no entrega nombres cortos | Al procesar el informe de la F1 |
| D-03 | Cómo interpretar `CONFIG.NT` y `AUTOEXEC.NT` | Descubrimiento de la F2 |
| D-04 | Base de código y licencias para Win16 (Wine, winevdm) | Descubrimiento de la F4 |
| D-05 | Qué hacer si upstream borra las rutas VDM de kernel32 o basesrv (el fork las conservaría) | En cada fusión semanal |

### 8.3 Overrides

| ID | Override | Estado |
|----|----------|--------|
| O-01 | La pregunta 12 del descubrimiento de la F1 quedó sin respuesta; se aplicó la opción recomendada (12a) | Pendiente de confirmar por Hector |

### 8.4 Riesgos abiertos

| ID | Riesgo | Impacto | Mitigación |
|----|--------|---------|------------|
| R-01 | Fast486 es un intérprete y puede quedarse corto para juegos | Alto en F2 y F3 | Medir en F1 con `bench16`; decidir en D-01 |
| R-02 | Con `condrv`, basesrv podría no emparejar la VDM con su consola por `ConsoleHandle` | Medio | VDM efímera (ADR-05 de la F1); comprobar con dos lanzamientos seguidos |
| R-03 | El NTFS nuevo de LiberNT podría no dar nombres 8.3 y el DOS no vería ficheros de nombre largo | Medio | Comprobarlo en F1; alias virtuales en D-02 |
| R-04 | Conflictos al fusionar upstream (unos 170 commits al día) en kernel32, rosconfig o CMake | Medio | Cambios mínimos y aislados (T-05); fusión semanal |
| R-05 | Upstream borra o rehace las rutas VDM de kernel32 o basesrv | Alto | Vigilarlo en cada fusión; D-05 |
| R-06 | El arnés propio no reproduce las condiciones del de upstream | Bajo | Validación manual complementaria en cada fase |
| R-07 | Licencias del corpus de pruebas | Bajo | Corpus fuera del repo; licencia en cada `CORPUS_ITEM`; FreeDOS es GPL |
| R-08 | Supuesto O-01 (12a) no confirmado | Bajo | Si cambia, solo afecta a pruebas y plan de la F1 |
| R-09 | Builds largas en tres arquitecturas y 7 trabajos de CI por push | Medio | Builds incrementales; `CCACHE_DISABLE=1` según `AGENTS.md`; pruebas locales |
