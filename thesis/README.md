# Thesis source

The current `main.tex` is a minimal `report`-class skeleton, intended as a
placeholder until a real UJ template is chosen. There is no widely-known
official UJ Wydział Matematyki i Informatyki LaTeX template in public repos.

## Templates to evaluate

1. **Completely Unofficial Jagiellonian University Thesis Template** —
   <https://www.overleaf.com/latex/templates/completely-unofficial-jagiellonian-university-thesis-template/fxnssjzrtpfk>.
   Designed for UJ; supports English and Polish. Current best candidate.
2. **Wzór ISI UJ** —
   <https://www.overleaf.com/latex/templates/wzor-isi-uj/ddszfvqhkwmt>.
   Targets UJ Institute of Information Studies, not WMiI; probably wrong faculty.
3. **Ask Lech Duraj** whether the Faculty of Mathematics and Computer Science
   has an internal recommendation.

To switch templates: replace `main.tex` and any class file; the per-chapter
files in `chapters/` should remain reusable as long as the new template uses
`\chapter`.

## Build

```bash
cd thesis
pdflatex main && bibtex main && pdflatex main && pdflatex main
```
