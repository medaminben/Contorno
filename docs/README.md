# Contorno documentation

| Document | For | Where |
|---|---|---|
| User manual, in English, Deutsch, Español, Italiano, Français, Nederlands and العربية | operators | `manual/index.html` (opened by the ? of the Set Up dialog); sources in `manual/content/<lang>/` |
| Architecture and structure | engineers who build, change or review Contorno | `architecture/architecture.md` |
| Scientific article: measuring the surface of animal hides from a single photograph | engineers, metrologists, the leather industry | `articles/contorno-area-measurement/contorno-area-measurement.pdf` (source `.md`, figures in `figures/`, scripts in `scripts/`) |
| Implementation paper: how Contorno Indicator is built, and the tools that build and check it | engineers | `articles/contorno-indicator/contorno-indicator.pdf` (source `.md`; `scripts/build_paper.sh`) |
| Sample images: origin, licence and use of every photo | anyone adding, publishing or citing a sample | `samples/image-sources.md` |

Rebuilding:

- Manual: edit `manual/content/<lang>/*.md`; `manual/build_manual.py` for the
  pages. `ctest` runs `build_manual.py --check`: every page in every language,
  links, and every label it quotes as the application shows it.
- Article: `docs/articles/contorno-area-measurement/`, with its scripts in `scripts/`:
  `scripts/run_experiment.sh` (measurements and figures), then
  `scripts/build_article.sh` (HTML and PDF).
- Implementation paper: `docs/articles/contorno-indicator/scripts/build_paper.sh`
  (HTML and PDF). It uses the article's stylesheet.
