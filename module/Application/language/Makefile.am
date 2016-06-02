all: update-pot update-all msgfmt-all

update-pot:
	@(echo 'Updating webui.pot ...';)
	@(find ../../ -name '*.ph*' | xargs xgettext --keyword=translate -L PHP --from-code=UTF-8 -o webui.pot;)

update-po: $(PO)
	@(echo 'Updating $(PO) file ...';)
	@(msgmerge -U $(PO) webui.pot);

update-all: webui.pot
	@(echo 'Updating all *.po files ...';)
	@(for i in de_DE en_EN fr_FR ru_RU; do msgmerge -U $$i.po $< && touch $$i.po; done;)

msgfmt-all: webui.pot
	@(echo 'Generating all *.mo files ...';)
	@(for i in de_DE en_EN fr_FR ru_RU; do msgfmt $$i.po --output-file=$$i.mo; done;)
