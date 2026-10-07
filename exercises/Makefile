# Build all exercise sheets with and without solutions.
#   make            -> pdf/SheetNN.pdf and pdf/SheetNN_solutions.pdf
#   make sheet05    -> only sheet 5 (both versions)
#   make book       -> pdf/Exercises_all.pdf and pdf/Exercises_all_solutions.pdf
#   make clean
SHEETS := $(sort $(basename $(notdir $(wildcard sheets/sheet[0-9][0-9].tex))))
LATEX  := pdflatex -interaction=nonstopmode -halt-on-error -output-directory=build

all: $(SHEETS)

$(SHEETS): %: sheets/%.tex dspex.sty sheet.tex
	@mkdir -p build pdf
	@n=$$(echo $@ | sed 's/sheet//'); \
	for pass in 1 2; do \
	  $(LATEX) -jobname=Sheet$$n "\def\sheetfile{sheets/$@}\input{sheet}" >/dev/null || \
	    { echo "ERROR in $@ (see build/Sheet$$n.log)"; exit 1; }; \
	  $(LATEX) -jobname=Sheet$${n}_solutions "\def\withsolutions{}\def\sheetfile{sheets/$@}\input{sheet}" >/dev/null || \
	    { echo "ERROR in $@ solutions (see build/Sheet$${n}_solutions.log)"; exit 1; }; \
	done; \
	cp build/Sheet$$n.pdf build/Sheet$${n}_solutions.pdf pdf/; echo "built $@"

book: $(addprefix sheets/,$(addsuffix .tex,$(SHEETS))) dspex.sty book.tex
	@mkdir -p build pdf
	$(LATEX) -jobname=Exercises_all book >/dev/null; $(LATEX) -jobname=Exercises_all book >/dev/null
	$(LATEX) -jobname=Exercises_all_solutions "\def\withsolutions{}\input{book}" >/dev/null
	$(LATEX) -jobname=Exercises_all_solutions "\def\withsolutions{}\input{book}" >/dev/null
	cp build/Exercises_all.pdf build/Exercises_all_solutions.pdf pdf/

clean:
	rm -rf build

.PHONY: all book clean $(SHEETS)
