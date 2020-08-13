#!/bin/bash
#remove all context calls from py files

for i in *py*; do
  #sed -i 's#context, ##g;s#, context##g;s#context,$##g' "$i";
  perl -i -p0e 's#context, ##g;s#, context##g;s#\n *context,##g' "$i";
done
