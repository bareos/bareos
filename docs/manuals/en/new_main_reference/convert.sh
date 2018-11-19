#!/bin/bash

# pandoc - convert tex to rst

./tex2rst.sh

# sed - replace raw-latex with rst

./replace.sh

exit
