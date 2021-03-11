#!/usr/bin/env python
# splits an existing bareos postgres dump into single files per table

import re

outfile = open("header", "w")

copy = re.compile("^COPY (\w+)")

for line in open('bareos.sql'):
    m = re.match(copy, line)
    if m:
      print line, m.group(1)
      outfile.close()
      outfile = open(m.group(1) + ".sql", "w")
      outfile.write(line)
    else: 
      outfile.write(line)
