# librent - Checklist Fase 1: NTVDM en 64 bits (DOS en modo texto)

## Fork y repositorio
- [x] Fork en GitHub de `ahmedarif193/LiberNT` creado con `gh` con el nombre `librent`, con la rama `main` creada desde `main-nt10` como rama por defecto
- [x] Remoto `upstream` apuntando a `ahmedarif193/LiberNT` y procedimiento de fusión semanal documentado en `CLAUDE.md`
- [x] Primera fusión de `upstream/main-nt10` hecha con el procedimiento documentado, con fecha y commit de upstream anotados en el informe de implementación
  - AC: Dado el fork, cuando se sigue el procedimiento de `CLAUDE.md`, entonces la fusión termina sin pasos que no estén documentados
- [ ] GitHub Actions activado en el fork, con `build.yml` ejecutándose en cada push y los workflows `stale` y `labeler` desactivados desde la configuración de GitHub (sin tocar sus ficheros)
- [x] `gh repo set-default` apuntando al fork, para que ninguna PR vaya a `ahmedarif193/LiberNT`
- [x] `CLAUDE.md` en la raíz y documentación de la fase en `docs/`
- [ ] `LIBRENT_VERSION` en la raíz con la versión `0.1.0` al cerrar la fase
- [ ] `NOVEDADES.md` en la raíz con la entrada de la versión `0.1.0`
- [ ] Documentación de usuario en `docs/usuario/dos.md`: cómo ejecutar programas DOS, la ventana y sus atajos, y los límites de la Fase 1
- [ ] Cambios en ficheros de upstream limitados a los del §2.3 del informe
  - AC: Dado `git diff --stat upstream/main-nt10...main`, cuando se revisa, entonces fuera de las rutas del fork solo aparecen los ficheros del §2.3 o los anotados como desviación

## Compilación
- [x] `subsystems/mvdm` restaurado desde `054ce27^` con ntvdm, dos, config, pifmgr y vdmdbg, sin `wow16` ni `samples`
- [x] Opción `ENABLE_NTVDM` en el menú System de menuconfig con valor `auto`: activa en i386, amd64 y arm64, desactivada en riscv64 y ppc
  - AC: Dado `./configure.sh -a riscv64`, cuando se configura, entonces NTVDM no se compila; con `-a amd64`, sí
- [x] `subsystems/CMakeLists.txt` incluye `mvdm` solo cuando `ENABLE_NTVDM` está activa, sin condición de compilador
- [x] `asm16` ensambla COMMAND.COM con el ensamblador integrado de Clang, sin `i686-w64-mingw32-as`
  - AC: Dado un PATH sin `i686-w64-mingw32-as`, cuando se compila con Clang, entonces se genera `command.com` y `hello.com` funciona con él
- [x] NTVDM compila con Clang y `-Werror` en amd64 Debug
- [x] NTVDM compila con GCC en i386 y amd64
- [x] NTVDM compila con Clang en arm64
- [x] Ningún `#ifdef` de arquitectura nuevo en `subsystems/mvdm` ni en `sdk/lib/fast486`
- [x] `IMAGEBASE` fija eliminada de la definición del módulo `ntvdm`
- [x] `ntvdm.exe`, `command.com`, `pifmgr.dll`, `vdmdbg.dll`, `CONFIG.NT` y `AUTOEXEC.NT` incluidos en `bootcd.iso`, `LiberNT.img` y `LiberNT.vhd`
- [ ] CI del fork en verde en los 7 trabajos de `build.yml`

## Memoria y 64 bits
- [x] Memoria del PC emulado (16 MB) reservada en cualquier dirección del proceso, nunca en la dirección 0
  - AC: Dada la VDM en amd64, cuando arranca, entonces `BaseAddress` es distinta de 0 y `hello.com` funciona
- [x] `BaseGetVdmConfigInfo` devuelve una reserva de tamaño 0 y kernel32 no reserva nada en el proceso hijo
- [x] `ntoskrnl/nvs` sin ningún cambio
- [x] 64 KB de guarda sin acceso detrás de la memoria del invitado
- [x] Conversiones de dirección del invitado a puntero del host (BOP, BIOS y DOS) validadas contra el tamaño de la memoria del invitado
  - AC: Dado un programa que pide a INT 21h leer en un búfer que cruza el final de la memoria del invitado, cuando se ejecuta, entonces la llamada falla con error DOS y la VDM sigue funcionando
- [x] Comprobación de `VdmAllowed` (`ProcessWx86Information`) mantenida al arrancar `ntvdm.exe`

## Ventana de la VDM
- [ ] Frontend de consola (`console/console.c`, `console/video.c`) eliminado y sustituido por el frontend de ventana en `ntvdm/frontend/`
- [ ] Hilo de ventana con bucle de mensajes propio, separado del hilo de emulación
- [ ] Modo texto pintado con la fuente del plano 2 de la VGA emulada
  - AC: Dado `textmodes.com`, cuando carga una fuente propia con INT 10h AX=1110h, entonces la ventana muestra sus glifos
- [ ] Ancho de celda de 8 o 9 puntos según la VGA emulada, con la novena columna replicada en los caracteres C0h–DFh
- [ ] Modos de texto 40×25, 80×25, 80×43 y 80×50
- [ ] Colores de los 16 atributos según la paleta DAC de la VGA emulada
- [ ] Parpadeo de atributo o fondo intenso según el registro de modo del controlador de atributos
- [ ] Cursor de texto con la forma, posición y visibilidad del CRTC
- [ ] Escalado entero automático (el mayor que cabe, hasta 4×) y fijo 1×, 2×, 3× y 4×
- [ ] Alt+Intro alterna pantalla completa sin cambiar el modo de vídeo del sistema
- [ ] Título de la ventana con el nombre del programa DOS
- [ ] Cerrar la ventana con el programa en marcha pide confirmación y lo termina
  - AC: Dado EDIT abierto, cuando se cierra la ventana y se confirma, entonces `ntvdm.exe` termina con `STATUS_CONTROL_C_EXIT`
- [ ] Al terminar el último programa DOS de la VDM, la ventana se cierra y el foco vuelve a la ventana anterior
- [ ] Lanzada desde el Explorador, no queda visible ninguna ventana de consola vacía
- [ ] Menú del sistema con Pantalla completa, Escala, Copiar texto, Pegar, Volcar memoria, Terminar la VDM y Acerca de la VDM
- [ ] «Acerca de la VDM» muestra la versión de librent (`LIBRENT_VERSION`) y la de LiberNT
- [ ] Escala y pantalla completa al iniciar guardadas en `HKCU\Software\librent\NTVDM`
- [ ] Superficie gráfica del frontend implementada (su validación es de la Fase 2)
- [ ] Textos nuevos de la ventana en `en-US` y `es-ES`, con la cadena inglesa en el resto de idiomas

## Entrada
- [ ] Teclas traducidas a códigos de exploración del conjunto 1, con prefijo E0 en las extendidas
- [ ] Caracteres según la distribución de teclado activa de Windows, convertidos a la página de códigos OEM 437
  - AC: Dado el teclado español, cuando se teclea «ñ» en EDIT y se guarda, entonces el fichero contiene el byte A4h y la ventana muestra «ñ»
- [ ] Estado de Ctrl, Alt, Mayús, Bloq Mayús y Bloq Num reflejado en el estado de teclado de la BIOS
- [ ] Alt+Intro no llega al programa DOS
- [ ] Ctrl+C en la consola de cmd mientras corre el programa llega al DOS como Ctrl+Pausa
- [ ] Ratón INT 33h con coordenadas absolutas y botones izquierdo, derecho y central
  - AC: Dado el modo 80×25, cuando el puntero está sobre la celda columna 10 y fila 5, entonces INT 33h AX=3 devuelve CX=80 y DX=40
- [ ] Puntero del sistema oculto sobre la ventana mientras el programa muestra su cursor de ratón
- [ ] Copiar texto: la pantalla entera al portapapeles en Unicode, convertida desde CP437
- [ ] Pegar: texto del portapapeles convertido a CP437 e inyectado en el búfer de teclado de la BIOS a medida que se vacía

## Integración con el sistema
- [x] Un `.COM` o `.EXE` de DOS lanzado desde cmd se ejecuta en una VDM y cmd espera a que termine
- [ ] Doble clic en un `.COM` o `.EXE` de DOS en el Explorador lo ejecuta en una VDM
- [ ] El código de salida del programa DOS llega a cmd como ERRORLEVEL
  - AC: Dado `exitcode.com 7`, cuando termina, entonces `echo %errorlevel%` muestra 7
- [ ] Redirección de salida (`>` y `>>`) de un programa DOS a fichero
- [ ] Redirección de entrada (`<`) y tuberías (`|`) con programas DOS
  - AC: Dado `type datos.txt | stdio.com > salida.txt`, cuando termina, entonces `salida.txt` contiene el texto de entrada en mayúsculas
- [x] VDM efímera: ejecuta las órdenes pendientes de su consola y termina cuando no le quedan, sin quedar residente
  - AC: Dado `hello.com` lanzado dos veces seguidas desde cmd, cuando terminan, entonces no queda ningún `ntvdm.exe` en ejecución
- [ ] Un programa DOS que lanza otro programa DOS (INT 21h 4Bh) lo ejecuta en la misma VDM y recibe su código de salida
- [ ] Un programa DOS que lanza un programa Win32 lo ejecuta y espera a que termine
- [ ] Un ejecutable de Windows 3.x (NE) muestra el aviso de Win16 y termina con 193 sin ejecutar el stub de DOS
- [ ] Con `ENABLE_NTVDM` desactivada, lanzar un programa DOS falla con `ERROR_BAD_EXE_FORMAT` y no deja registros en basesrv
- [ ] Con la directiva `VDMDisallowed` activa, lanzar un programa DOS falla con `ERROR_VDM_DISALLOWED`
- [ ] El programa DOS hereda el directorio actual y las unidades de cmd
- [ ] El programa DOS recibe las variables de entorno de cmd, con rutas cortas

## DOS y hardware emulado
- [x] COMMAND.COM integrado ejecuta el programa pedido y devuelve su código de salida
- [ ] INT 21h de ficheros (crear, abrir, leer, escribir, mover el puntero, cerrar, borrar, renombrar, atributos) sobre NTFS y FAT
- [ ] INT 21h de directorios (crear, borrar, cambiar, obtener el actual) y búsqueda FindFirst/FindNext con nombres 8.3
- [ ] Comprobado si el NTFS de LiberNT entrega nombres cortos 8.3, con el resultado anotado en el informe de implementación
- [ ] Nombres de fichero en la página de códigos OEM 437
  - AC: Dado `filetest.com`, cuando crea `AÑO.TXT`, entonces el Explorador muestra `AÑO.TXT`
- [ ] INT 21h de memoria (asignar, liberar, redimensionar) y de procesos (4Bh, 4Ch, 4Dh)
- [ ] Fecha y hora DOS (INT 21h 2Ah y 2Ch, INT 1Ah) iguales a las del sistema
- [ ] XMS 3.0: detección, consulta de memoria libre, asignación, movimiento, liberación, A20 y HMA
- [ ] EMS 4.0: detección, marco de página, asignación, mapeo, guardar y restaurar, liberación
- [ ] Altavoz: tonos del canal 2 del PIT y del puerto 61h por `\Device\Beep`
- [ ] Reposo de la CPU emulada con HLT, INT 16h bloqueante con el búfer vacío, INT 28h e INT 2Fh AX=1680h
  - AC: Dada la prueba `idle` (EDIT sin tocar durante 10 s), cuando termina, entonces `cpu_ms` es menor que el 10 % de `wall_ms`
- [ ] Una excepción del invitado sin manejador muestra un aviso con CS:IP y permite terminar
- [ ] `CONFIG.NT` y `AUTOEXEC.NT` instalados en `system32` (su interpretación es de la Fase 2)

## Pruebas y rendimiento
- [x] `scripts/librent/vmtest.py` en Python 3 sin dependencias externas, con las órdenes `payload`, `run`, `report` y `all`
- [x] `vmtest.py run` arranca `LiberNT.img` en QEMU/KVM sin pantalla, con `-snapshot`, salida serie a fichero, QMP y el disco de pruebas como segunda unidad
- [x] Opción `ENABLE_NTVDM_BOOT_RUN` (solo Debug) que al iniciar sesión busca `\LIBRENT\RUNTESTS.CMD` de C: a Z: y lo ejecuta
- [x] `vmtestmark.exe` emite las marcas `LIBRENT-VMTEST-KEYS` y `LIBRENT-VMTEST-DONE` por la salida de depuración hacia el puerto serie
- [ ] Programas de prueba propios en `modules/rostests/tests/ntvdm16/`: hello, exitcode, stdio, filetest, memtest, xmstest, emstest, mousetest, speaker, idle, keytest, textmodes y bench16
- [ ] Corpus externo en `scripts/librent/corpus/manifest.json` con EDIT, MEM, FIND, MORE, SORT, CHOICE y TREE de FreeDOS, cada uno con URL, SHA-256 y licencia, y binarios en una caché fuera del repo
  - AC: Dado un SHA-256 que no coincide, cuando se ejecuta `vmtest.py payload`, entonces el elemento se rechaza y la orden termina con error
- [ ] Ficheros de referencia de salida y de pantalla en `scripts/librent/corpus/golden/`
- [ ] Volcado de pantalla de texto (UTF-8 y atributos) al terminar el programa
- [ ] Volcado de pantalla adicional a los milisegundos pedidos desde el inicio del programa
- [ ] Estadísticas al terminar: instrucciones, MIPS, tiempos de CPU y de reloj, reposo, arranque y eventos del altavoz
- [ ] Modo sin diálogos para pruebas: los avisos van a stderr y a las estadísticas, y cerrar no pide confirmación
- [ ] Teclas inyectadas por QMP en las pruebas interactivas (EDIT y keytest)
- [x] `vmtest.py report` compara con las referencias, imprime una tabla y sale con error si alguna prueba falla
- [ ] Medición de Fast486 con `bench16` en amd64 sobre QEMU/KVM, con MIPS medios y su clasificación según §7.8 anotados en el informe de implementación
- [ ] Arranque de la VDM por debajo de 2 s (`startup_ms` de `hello`) en amd64 Debug sobre QEMU/KVM
- [ ] Todas las pruebas automáticas en verde en amd64 Debug con Clang
- [ ] Pruebas manuales hechas y anotadas: teclado español, ratón real en EDIT, pantalla completa, copiar y pegar, doble clic en el Explorador y memoria del proceso por debajo de 64 MB

## Interfaces
- [ ] `VgaConsoleInitialize` y `VgaConsoleCleanup` crean y cierran la ventana y su hilo
- [ ] `VgaConsoleCreateTextScreen` y `VgaConsoleDestroyTextScreen` sobre la rejilla de `CHAR_CELL`
- [ ] `VgaConsoleCreateGraphicsScreen` y `VgaConsoleDestroyGraphicsScreen` sobre un DIB indexado con paleta
- [ ] `VgaConsoleRepaintScreen` invalida el rectángulo y pinta el hilo de ventana
- [ ] `VgaConsoleUpdateTextCursor` actualiza forma, posición y visibilidad del cursor
- [ ] `VgaAttachToConsole` y `VgaDetachFromConsole` muestran y ocultan la consola para los programas Win32 lanzados desde DOS
- [ ] `VgaGetDoubleVisionState` sin cambios de comportamiento
- [ ] `ScreenEventHandler` eliminado
- [ ] `KeyboardEventHandler`, `MouseEventHandler` y `FocusEventHandler` llamados desde el hilo de ventana
- [ ] `CreateProcessInternalW`, `BaseCheckVDM` y `BaseCheckForVDM` verificados sin cambios en la ruta VDM
- [ ] `BaseGetVdmConfigInfo` con reserva 0 y `ERROR_BAD_EXE_FORMAT` si falta `ntvdm.exe`
- [ ] `GetNextVDMCommand` y `ExitVDM` siguiendo el modelo de VDM efímera
- [ ] `ENABLE_NTVDM` en `rosconfig.def` y `config.cmake`
- [ ] `ENABLE_NTVDM_BOOT_RUN` en el menú Testing (solo Debug)
- [ ] `HKCU\Software\librent\NTVDM` con `WindowScale`, `StartFullScreen`, `WindowX` y `WindowY`
- [ ] `HKLM\SYSTEM\CurrentControlSet\Control\NTVDM` leído sin cambios
- [ ] `VDMDisallowed` respetado
- [ ] `NTVDM_STATS` escribe el fichero de estadísticas
- [ ] `NTVDM_SCREEN_DUMP` y `NTVDM_SCREEN_DUMP_MS` escriben los volcados de pantalla
- [ ] `NTVDM_NO_DIALOGS` desactiva los diálogos
- [x] `vmtest.py payload`
- [x] `vmtest.py run`
- [x] `vmtest.py report`
- [x] `vmtest.py all`

---

**Progreso Fase 1:** 33 / 122 funcionalidades

**Total librent (Fases 1-1):** 33 / 122 funcionalidades
