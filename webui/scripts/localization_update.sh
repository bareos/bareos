#
# This script serves as a helper to keep our POT and PO files up to date after source code changes.
#
#!/bin/sh

POTFILE='../module/Application/language/webui.pot'
LOCDIR='../module/Application/language/'

echo 'Message lookup ...'

find ../ -regextype posix-egrep -regex '.*(php|phtml)$$' | grep -v vendor | grep -v tests | xargs xgettext --keyword=translate -L PHP --from-code=UTF-8 -o $POTFILE;
find ../ -regextype posix-egrep -regex '.*(functions.js)$$' | xargs xgettext --keyword=gettext --from-code=UTF-8 -j -o $POTFILE;

echo 'Message merge ...'
cd $LOCDIR
for i in cn_CN cs_CZ de_DE en_EN es_ES fr_FR hu_HU it_IT nl_BE pl_PL pt_BR ru_RU sk_SK tr_TR; do echo $i && msgmerge --backup=none -U $i.po webui.pot && touch $i.po; done;

#echo 'Message format ...'
#for i in cn_CN cs_CZ de_DE en_EN es_ES fr_FR hu_HU it_IT nl_BE pl_PL pt_BR ru_RU sk_SK tr_TR; do echo $i && msgfmt $i.po --output-file=$i.mo; done;

echo 'Done'
