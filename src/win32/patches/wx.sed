s%config.gcc%config.mingw32%
s%\\\(.\)%/\1%g
s%ranlib%mingw32-ranlib%
s%windres%mingw32-windres%
s%ar rc%mingw32-ar rc%
s%makefile\.gcc%makefile\.mingw32%
s%if exist \([^ ][^ ]*\) del \1%if [ -e \1 ]; then rm \1; fi%
s%if not exist \([^ ][^ ]*\) mkdir \1%if [ ! -e \1 ]; then mkdir \1; fi%
s%if not exist \([^ ][^ ]*\) copy \([^ ][^ ]*\) \1%if [ ! -e \1 ]; then cp \2 \1; fi%
