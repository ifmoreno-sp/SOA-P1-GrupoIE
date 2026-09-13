# Informe (fuente LaTeX)

Fuente del informe del proyecto (`docs/informe.pdf`). El enunciado solo pide
el PDF; esta carpeta con la fuente `.tex` se mantiene por conveniencia
(historial de cambios, colaboración), no porque el enunciado la exija.

## Compilar

```bash
cd docs/informe
pdflatex -interaction=nonstopmode main.tex
bibtex main
pdflatex -interaction=nonstopmode main.tex
pdflatex -interaction=nonstopmode main.tex   # segunda pasada: resuelve referencias cruzadas
cp main.pdf ../informe.pdf
```

## Regenerar los datos de las gráficas

`graficas/generar_datos_proporcionalidad.py` corre el binario real
(`./lottery_scheduler`, ya compilado en la raíz del repo) para regenerar
`graficas/objetivo_vs_observado.dat` y `graficas/convergencia_caso4.dat` a
partir de una corrida real del experimento de proporcionalidad (Caso 4):

```bash
make all   # desde la raiz del repo, si no esta compilado
python3 docs/informe/graficas/generar_datos_proporcionalidad.py
```
