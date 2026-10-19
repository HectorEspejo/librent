# Prompt librent - Fase 1: NTVDM en 64 bits (DOS en modo texto)

Desarrolla **librent**, fork propio de LiberNT (NT libre derivado de ReactOS), empezando por devolver NTVDM, borrado en `054ce27`, y hacerlo funcionar en 64 bits para ejecutar programas DOS de texto sin máquina virtual. Stack: **C al estilo LiberNT, CMake + Ninja con RosBE (Clang y GCC), Fast486, Win32, kernel32 + basesrv (CSR), Python 3, QEMU/KVM, QMP y mtools**. Estructuras: (1) **VDM_CONSOLE_RECORD** y (2) **VDM_DOS_RECORD** de basesrv, sin cambios; (3) **VDM_GUEST_MEMORY**, 16 MB reubicables con 64 KB de guarda; (4) **VDM_FRONTEND**, estado de la ventana; (5) **CHAR_CELL**; (6) **VDM_SETTINGS** en `HKCU\Software\librent\NTVDM` (WindowScale, StartFullScreen); (7) **NTVDM_MACHINE_CONFIG**, sin cambios; (8) **NTVDM_STATS** y (9) **NTVDM_SCREEN_DUMP**, ficheros de prueba; (10) **CORPUS_ITEM** en `scripts/librent/corpus/manifest.json`; (11) **TEST_RESULT**. Funcionalidades: fork con fusión semanal de `main-nt10`, restaurar `subsystems/mvdm` desde `054ce27^` tras `ENABLE_NTVDM` (auto en i386, amd64 y arm64), `asm16` con el ensamblador integrado de Clang, nada en la dirección 0 y `BaseGetVdmConfigInfo` sin reserva, VDM efímera por consola, cmd y Explorador transparentes, redirecciones, ERRORLEVEL, aviso Win16 con salida 193, COMMAND.COM, ficheros 8.3 en CP437, XMS, EMS, INT 33h absoluto, altavoz por `\Device\Beep` y reposo sin consumo. Interfaz: ventana propia en lugar de `console/`, fuente del plano 2 de la VGA, celdas de 8 o 9 puntos, escalado entero, Alt+Intro, teclado en CP437, copiar y pegar, menú del sistema con «Acerca de» y versión, textos en `en-US` y `es-ES`. Pruebas: `vmtest.py` (payload, run, report, all), `ENABLE_NTVDM_BOOT_RUN`, programas propios de 16 bits, utilidades de FreeDOS verificadas por SHA-256 fuera del repo, ganchos `NTVDM_STATS`, `NTVDM_SCREEN_DUMP`, `NTVDM_SCREEN_DUMP_MS` y `NTVDM_NO_DIALOGS`, y MIPS de Fast486 con `bench16`.

---

## Instrucciones para el agente

1. Lee primero `CLAUDE.md` en la raíz del repositorio: contiene las reglas
   permanentes de trabajo (idioma, commits, effort, cierre de fase) y qué
   reglas de `AGENTS.md` sustituye.
2. Sigue el checklist `docs/librent-fase1-checklist.md` como definición
   del alcance. No añadas funcionalidades fuera de él sin indicarlo. El
   informe `docs/librent-fase1-informe.md` tiene el detalle (estructuras,
   flujos, interfaces y decisiones).
3. Al finalizar el desarrollo o al pausar la sesión, marca el checklist y
   genera/actualiza el archivo **`docs/librent-fase1-implementacion.md`**
   (informe de implementación) con exactamente esta estructura:
   - Resumen de lo implementado
   - Desviaciones respecto a la especificación (qué y por qué)
   - Estructura de archivos creada/modificada
   - Decisiones técnicas tomadas durante el desarrollo
   - Funcionalidades del checklist completadas (copiando su texto exacto)
   - Pendientes y bloqueos
   - Ejecución y pruebas (cómo arrancar, migrar y testear)
4. Actualiza el informe de implementación en cada sesión, no lo
   regeneres desde cero.
5. Cada vez que implementes funcionalidad nueva: añade la documentación de
   usuario en `docs/usuario/`, sube la versión en `LIBRENT_VERSION` y
   escribe sus novedades en `NOVEDADES.md`.
6. Cuando el informe esté listo, avisa al desarrollador de que debe
   entregárselo al analista funcional: hasta entonces la documentación de
   proyecto (checklist maestro, informe maestro) no refleja la realidad.
