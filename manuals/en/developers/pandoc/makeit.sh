#!/bin/bash

FILES="generaldevel.md\
      git.md\
      pluginAPI.md\
      platformsupport.md\
      daemonprotocol.md\
      director.md\
      file.md\
      storage.md\
      catalog.md\
      mediaformat.md\
      porting.md\
      gui-interface.md\
      tls-techdoc.md\
      regression.md\
      md5.md\
      mempool.md\
      netprotocol.md\
      smartall.md\
      fdl.md"

pandoc -s -S --toc -c pandoc.css -B footer.html -A footer.html  $FILES -o developers.html

