all: update-pot update-all msgfmt-all

update-pot:
	@(echo 'Updating webui-datatables.pot ...';)
	@(find ../../ -regextype posix-egrep -regex '.*(functions.js)$$' | xargs xgettext --keyword=gettext --from-code=UTF-8 -o webui-datatables.pot;)

update-po: $(PO)
	@(echo 'Updating $(PO) file ...';)
	@(msgmerge --backup=none -U $(PO) webui-datatables.pot)

update-all: webui-datatables.pot
	@(echo 'Updating all *.po files ...';)
	@(for i in cn_CN de_DE en_EN es_ES fr_FR it_IT ru_RU; do msgmerge --backup=none -U $$i/LC_MESSAGES/$$i.po $< && touch $$i/LC_MESSAGES/$$i.po; done;)

msgfmt-all: webui-datatables.pot
	@(echo 'Generating all *.mo files ...';)
	@(for i in cn_CN de_DE en_EN es_ES fr_FR it_IT ru_RU; do msgfmt $$i/LC_MESSAGES/$$i.po --output-file=$$i/LC_MESSAGES/$$i.mo; done;)
