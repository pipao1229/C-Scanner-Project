# C Lexical Scanner Using Flex

This project creates an end-to-end lexical analyzer for the full C programming language implemented in C11 and Flex. 

The pipeline takes any source file, runs an integrated preprocessing phase to strip comments and resolve directives (`#define`, `#include`), tokenizes the input using `Get_Token()`, and automatically compiles and displays a Beamer presentation featuring syntax-highlighted source code, token statistics, PGFPlots histograms, and distribution pie charts.

---

## Authors

* **Felipe Benavides**
* **Matthew Gaviria Brenes**
* **Marvin Zamora Mussio**

**Institution:** Instituto Tecnológico de Costa Rica (TEC)  
**Course:** Compiladores e Intérpretes  
**Term:** Semestre I - 2026  

---

## Requirements & Dependencies

The project runs natively on Linux and requires standard build tools: Flex, LaTeX (Beamer, TikZ, PGFPlots), and the Evince PDF viewer.

### Ubuntu / Debian / Linux Mint

```bash
sudo apt update
sudo apt install -y build-essential flex evince \
    texlive-latex-base \
    texlive-latex-recommended \
    texlive-latex-extra \
    texlive-pictures \
    texlive-fonts-recommended
```

---

## Compilation

The project uses a standard Makefile. To build the executable:

```bash
make clean
make
```

This generates the binary executable `./scanner`.

---

## Usage

```bash
./scanner <input-file>
```

The scanner accepts input files with any extension.

---

## Architecture Overview

### Preprocessing
* Strips single-line (`//`) and multi-line (`/* ... */`) comments while preserving newlines for accurate line tracking.
* Expands `#define` object-like macros recursively, protected against self-referential expansion loops.
* Resolves nested `#include` directives with cycle detection to prevent stack overflow.
* Outputs clean preprocessed source to a non-hardcoded secure temporary file in `/tmp`.

### Lexical Scanning
* Implemented using Flex regular expressions covering the official C standard.
* Provides the decoupled `Get_Token()` API function.
* Categorizes lexemes into:
  * `KEYWORD`
  * `IDENTIFIER`
  * `INTEGER_LITERAL`
  * `FLOAT_LITERAL`
  * `STRING_LITERAL`
  * `CHAR_LITERAL`
  * `OPERATOR`
  * `DELIMITER`
  * `LEXICAL_ERROR`

### Presentation Generator
* Emits a LaTeX Beamer presentation styled with the Madrid theme.
* Syntax-highlights the preprocessed code using dynamic color boxes.
* Uses smart pagination to prevent splitting functions across slides.
* Renders frequency histograms and pie charts with PGFPlots.
* Performs a double-pass `pdflatex` compilation to ensure exact slide count resolution.