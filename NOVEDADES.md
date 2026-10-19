# Novedades de librent

## 0.1.0 (en desarrollo)

### Programas de DOS en 64 bits

- librent vuelve a ejecutar programas de DOS (`.COM` y `.EXE` en modo real) en
  sistemas de 64 bits, algo que las versiones de 64 bits de Windows no hacen.
- Un programa de DOS arranca con doble clic desde el Explorador o escribiendo su
  nombre en `cmd`, sin máquina virtual ni emulador de PC externo.
- El programa muestra su ventana propia con los colores y los caracteres de la
  VGA emulada, incluida la fuente que cargue el propio programa.
- La ventana admite escalado (automático o fijo de 1× a 4×) y pantalla completa
  con Alt+Intro, y recuerda el ajuste entre ejecuciones.
- El menú de la ventana (Alt+Espacio) reúne pantalla completa, escala, copiar y
  pegar, volcado de memoria, terminar la VDM y «Acerca de», con las versiones
  de librent y LiberNT.
- El teclado responde a la distribución activa de Windows y se traduce a la
  página de códigos 437, la de los programas de DOS, de modo que la `ñ` y los
  acentos se escriben y se ven bien.
- Se puede copiar la pantalla al portapapeles y pegar texto en el programa, y
  el ratón funciona como ratón de DOS (INT 33h).
- El código de salida del programa llega a `cmd` como `%errorlevel%`, y las
  redirecciones (`>`, `>>`, `<` y tuberías) funcionan con los programas de DOS.
- Los programas disfrutan de memoria XMS y EMS, del altavoz del sistema y de un
  modo de reposo que no consume CPU mientras esperan.
- Las aplicaciones de Windows de 16 bits avisan de que todavía no están
  soportadas (llegan en una fase posterior) y terminan sin ejecutarse.
- Esta primera versión cubre los programas de modo texto; los gráficos, el
  sonido de las tarjetas de sonido y los juegos llegan en fases siguientes.
