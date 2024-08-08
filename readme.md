# md2html

This is a simple [Markdown](https://en.wikipedia.org/wiki/Markdown) to [HTML](https://en.wikipedia.org/wiki/HTML) converter.

It targets a subset of Markdown.

## Setup

Just compile md2html.c

```bash
gcc md2html.c -o md2html
```

## Usage

```bash
md2html [-i <input.md>] [-o <output.html>]
```

If the input file is not specified, the program will use stdin as input. This way one can so such things:


```bash
cat myfile.txt | md2html -o index.html
```

If the output file is not specified, the program will use stdout.

```bash
md2html -i index.md >> index.html
```
