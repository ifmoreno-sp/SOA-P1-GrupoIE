# SOA-P1-GrupoIE
Planificador por lotería en espacio de usuario (C17, POSIX pthreads). Proyecto 1 — MC 6004 Sistemas Operativos Avanzados, TEC, II semestre 2026.

Implementa la política de *proportional-share scheduling* descrita por
Waldspurger y Weihl: en cada despacho se sortea un boleto entre las tareas
READY y su propietaria recibe el siguiente intervalo de ejecución. No se
modifica el kernel ni se sustituye el scheduler de GNU/Linux.

## Integrantes

| Nombre | Carné | Usuario de GitHub |
|---|---|---|
| Emmanuel Barrantes Vargas | 200012366 | `ebarrantes07` |
| Isaac Moreno Fuentes | 2018119181 | `ifmoreno-sp` |
