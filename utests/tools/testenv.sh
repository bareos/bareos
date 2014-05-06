#!/bin/sh

top_srcdir=
coverage=no

# Parse arguments
while [ $# -gt 0 ]; do
    case "$1" in
    --coverage) coverage=yes ;;
    --top-srcdir) shift; top_srcdir="$1" ;;
    --) shift; break ;;
    -*) echo "Unknown option: $1" 1>&2 ; exit 1 ;;
    *) break ;;
    esac
    shift
done

# echo "coverage=$coverage"
# echo "top_srcdir=$top_srcdir"
# echo "\$@=" "$@"
# exit 1

if [ $coverage = yes -a -n "$top_srcdir" ]; then
    echo -n "Removing test coverage data files..."
    n=`find "$top_srcdir" -type f -name '*.gcda' -exec rm '{}' \; -print | wc -l`
    echo "$n files"
fi

# Under normal circumstances this is where we would invoke Valgrind.
# However libtool makes it extremely difficult to locate the correct
# shared libary *and* run through Valgrind at the same time.  So
# there's an awful hack in utest_main.c to start Valgrind from inside
# the process.

exec "$@"
