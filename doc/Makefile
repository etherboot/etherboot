VERSION=	5.0.11

DOCFILES+=	16 COPYING COPYING_compressor userman devman atnetboot compressor
DOCFILES+=	diskless freebsd old-README security spec sshterminal vendortags
DOCFILES+=	lilo+etherboot
HTMLDOC=	$(patsubst %,html/%.html,$(DOCFILES))
TEXTDOC=	$(patsubst %,text/%.txt,$(DOCFILES))
DVIDOC=		$(patsubst %,dvi/%.dvi,$(DOCFILES))
PSDOC=		$(patsubst %,ps/%.ps,$(DOCFILES))

all:	sgml/userman.sgml $(HTMLDOC) $(TEXTDOC)

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

sgml/userman.sgml:	sgml/userman.xsgml sgml/Options ../src/NIC sgml/Copyrights
	m4 -P sgml/userman.xsgml > sgml/userman.sgml

sgml/Options:	../src/Config
	sed -e '1,/<OptionDescription>/d' -e '/<\/OptionDescription>/,$$d' \
		-e 's/^#	//' -e 's/^# *//' ../src/Config > sgml/Options

tarball:
	(cd ..; tar cf /tmp/etherboot-doc-$(VERSION).tar doc)
	bzip2 -9 < /tmp/etherboot-doc-$(VERSION).tar > /tmp/etherboot-doc-$(VERSION).tar.bz2
	gzip -9 < /tmp/etherboot-doc-$(VERSION).tar > /tmp/etherboot-doc-$(VERSION).tar.gz

clean:
	rm -f html/*.html text/*.txt dvi/*.dvi ps/*.ps sgml/*.tex
