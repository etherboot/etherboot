DOCFILES+=	16 COPYING COPYING_compressor README atnetboot compressor
DOCFILES+=	diskless freebsd old-README security spec sshterminal vendortags
HTMLDOC=	$(patsubst %,html/%.html,$(DOCFILES))
TEXTDOC=	$(patsubst %,text/%.txt,$(DOCFILES))
DVIDOC=		$(patsubst %,dvi/%.dvi,$(DOCFILES))
PSDOC=		$(patsubst %,ps/%.ps,$(DOCFILES))

all:	sgml/README.sgml $(HTMLDOC) $(TEXTDOC)

latex:	$(DVIDOC) $(PSDOC)

ps/%.ps:	dvi/%.dvi
	dvips -o $@ $<

dvi/%.dvi:	sgml/%.tex
	cd dvi; latex "\\nonstopmode\\input{../$<}"; \
	latex "\\nonstopmode\\input{../$<}"; \
	rm -f *.aux *.log *.toc *.tex
	
html/%.html:	sgml/%.sgml
	cd html; rm -f $*-*.html; sgml2html ../$<

text/%.txt:	sgml/%.sgml	
	cd text; sgml2txt ../$<

# sgml/*.tex are intermediate files and are automatically deleted
sgml/%.tex:	sgml/%.sgml
	cd sgml; sgml2latex -o tex ../$<; \

sgml/README.sgml:	sgml/README.xsgml ../src/NIC sgml/Copyrights
	m4 -P sgml/README.xsgml > sgml/README.sgml

clean:
	rm -f html/*.html text/*.txt dvi/*.dvi ps/*.ps sgml/*.tex

