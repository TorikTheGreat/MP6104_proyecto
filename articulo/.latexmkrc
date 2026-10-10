# Compilación del artículo: pdflatex + bibtex (IEEEtran.bst)
$pdf_mode      = 1;          # pdflatex
$bibtex_use    = 2;          # corre bibtex y limpia el .bbl
$out_dir       = 'build';
$pdflatex      = 'pdflatex -interaction=nonstopmode -halt-on-error -file-line-error %O %S';
$clean_ext     = 'synctex.gz run.xml bbl';
