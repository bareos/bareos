#!/bin/bash

SOURCEFILE=$1
DESTFILE=$2

  cat ${SOURCEFILE}\
    | perl -pe 's#\\url\{(.*)\}#\\verb\|URL:\1\|#g' \
    | perl -pe 's#\{bmessage\}#{verbatim}#g' \
    | perl -pe 's#registrykey#textbf#g' \
    | perl -pe 's#HKEY_LOCAL_MACHINE#HKEY\\_LOCAL\\_MACHINE#g' \
\
    | perl -pe 's#\\begin\{logging\}#\\begin{verbatim}{logging}#g' \
    | perl -pe 's#\\end\{logging\}#\\end{verbatim}#g' \
\
    | perl -pe 's#\\begin\{config\}#\\begin{verbatim}{config}#g' \
    | perl -pe 's#\\end\{config\}#\\end{verbatim}#g' \
\
    | perl -pe 's#\\begin\{bconfig\}#\\begin{verbatim}{bconfig}#g' \
    | perl -pe 's#\\end\{bconfig\}#\\end{verbatim}#g' \
\
    | perl -pe 's#\\begin\{commands\}#\\begin{verbatim}{commands}#g' \
    | perl -pe 's#\\end\{commands\}#\\end{verbatim}#g' \
\
    | perl -pe 's#\\begin\{bareosConfigResource\}#\\begin{verbatim}{bareosConfigResource}#g' \
    | perl -pe 's#\\end\{bareosConfigResource\}#\\end{verbatim}#g' \
\
    | perl -pe 's#\\begin\{commandOut\}#\\begin{verbatim}{commandOut}#g' \
    | perl -pe 's#\\end\{commandOut\}#\\end{verbatim}#g' \
\
    | perl -pe 's#\\begin\{bconsole\}#\\begin{verbatim}{bconsole}#g' \
    | perl -pe 's#\\end\{bconsole\}#\\end{verbatim}#g' \
\
    | perl -pe 's#\\variable\{\$#\\textbf\{\\\$#g' \
    | perl -pe 's#sec:#section-#g' \
    | perl -pe 's#\$CONFIGDIR#CONFIGDIR#g'\
    | perl -pe 's#\$COMPONENT#COMPONENT#g'\
    | perl -pe 's#\$RESOURCE#RESOURCE#g'\
    | perl -pe 's#\$NAME#NAME#g'\
    | perl -pe 's#\$PATH#PATH#g'\
    | perl -pe 's#\\hide\{\$\}##g' \
    | perl -pe 's#\\path\|#\\verb\|path:#g' \
    | perl -pe 's#\\hfill##g' \
    | perl -pe 's#\\hypertarget#\textbf#g' \
    | perl -pe 's#\\bquote\{\\bconsoleOutput\{Building directory tree ...\}\}#\\bquote{Building directory tree ...}#g' \
    > ${DESTFILE}

  diff  --color -ruN ${SOURCEFILE} ${DESTFILE}
