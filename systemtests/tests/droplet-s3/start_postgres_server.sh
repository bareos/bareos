#!/bin/bash

working="working"

. environment

killall -SIGTERM postgres > /dev/null 2>&1

database_data_dir="$working"/pgsql/data

if [ -d "$database_data_dir" ]; then
  rm -rf "$database_data_dir"
fi

mkdir -p "$database_data_dir" || exit 1
chown postgres "$database_data_dir" || exit 2

sudo --login --user=postgres bash << EOF
initdb -D "$database_data_dir" || exit 3
postgres -D "$database_data_dir" > /dev/null 2>&1 &

while ! psql -l > /dev/null 2>&1; do
  sleep 0.1
done

createuser --superuser root || exit 4
EOF

exit 0

