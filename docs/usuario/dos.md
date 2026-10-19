# Programas de DOS en librent

librent puede ejecutar programas de DOS (ficheros `.COM` y `.EXE` de modo real)
en sistemas de 64 bits, cada uno en su propia ventana y sin ninguna máquina
virtual de por medio.

## Cómo lanzar un programa

- **Desde cmd**: escribe el nombre del programa como cualquier otra orden, por
  ejemplo `hello.com`. cmd espera a que termine y su código de salida queda en
  `%errorlevel%`.
- **Desde el Explorador**: haz doble clic en el `.COM` o `.EXE`. Al terminar el
  programa, la ventana se cierra sola y el foco vuelve a la ventana anterior.

El programa hereda la unidad y el directorio actuales de cmd, además de sus
variables de entorno (con nombres cortos 8.3).

## La ventana de la VDM

- El título muestra el nombre del programa, por ejemplo
  `EDIT — VDM de librent`.
- El contenido se dibuja con la fuente de la VGA emulada, así que se ven los
  caracteres y colores que el propio programa define; si el programa carga su
  propia fuente (INT 10h 1110h), la ventana la usa.
- El cursor de texto parpadea igual que en una pantalla VGA real.
- El escalado es siempre de píxeles enteros, para que la letra quede nítida.
  Con escala automática se usa el mayor factor que quepa (hasta 4×).

## Atajos y menú

- **Alt+Intro**: alterna entre ventana y pantalla completa, sin cambiar la
  resolución del sistema.
- **Alt+Espacio**: abre el menú del sistema de la VDM, con:
  - Pantalla completa
  - Escala: Automática, 1×, 2×, 3× y 4×
  - Copiar texto (toda la pantalla, en Unicode)
  - Pegar (el texto del portapapeles se escribe en el programa)
  - Volcar memoria: texto o binario
  - Terminar la VDM
  - Acerca de la VDM… (versiones de librent y LiberNT)
- **Alt+F4** o el botón de cerrar: si el programa sigue en marcha, pide
  confirmación antes de terminarlo.

## Teclado y ratón

- Las teclas siguen la distribución activa de Windows y se convierten a la
  página de códigos OEM 437 de DOS, así que la `ñ` y los acentos se escriben y
  se ven correctamente.
- El ratón funciona como ratón de DOS (INT 33h) con los botones izquierdo,
  derecho y central. Mientras un programa muestra su propio cursor, el puntero
  del sistema se oculta sobre la ventana.

## Ajustes

La ventana recuerda entre ejecuciones la escala, si estaba en pantalla
completa y su posición, en `HKCU\Software\librent\NTVDM`:

| Valor | Descripción |
|---|---|
| `WindowScale` | 0 automática; 1 a 4 fija |
| `StartFullScreen` | 1 = arrancar en pantalla completa |
| `WindowX`, `WindowY` | Última posición de la ventana |

## Límites de esta versión

- Solo se admiten programas de **modo texto**. Los gráficos, el sonido de las
  tarjetas de sonido y los juegos llegan en una fase posterior.
- Las aplicaciones de **Windows de 16 bits** avisan de que aún no están
  soportadas y terminan sin ejecutarse.
- El intérprete `COMMAND.COM` es interno y no interactivo: ejecuta el programa
  pedido y devuelve su código de salida.
- Los extensores de DOS en modo protegido (DOS/4GW, DPMI) aún no están
  soportados.
- Duración o retardo de teclas, disquetes e imágenes de disco desde la ventana
  llegan en fases siguientes.
