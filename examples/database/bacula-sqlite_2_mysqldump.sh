#! /bin/sh
#
# bacula-sqlite_2_mysqldump.sh
#
# Convert a Bacula 1.36.2 Sqlite database to MySQL
# Originally Written by Nic Bellamy <nic@bellamy.co.nz>, Sept/Oct 2003.
# Modified by Silas Bennett <silas.bennett_AT_ge.com>, April 2006 for use with Bacula 1.36.2
#

if [ $1 = '-h' ] || [ $1 = '--help' ] ; then
	echo `basename "$0"`" Usage:"
	echo "	"`basename $0`" takes a ASCII bacula sqlite database dump as an argument,"
	echo "	and writes an SQL dump suitable for use with MySQL to STDOUT."
	echo
	echo "	Example Use:	"`basename $0`" bacula.sqlite.sql > bacula.mysql.sql"
	echo "	Example Use:	"cat bacula.sqlite.sql | `basename $0`" - | mysql -p -u <user> baculadb"
	exit
fi

# If $1 is '-' then cat will read /dev/stdin
cat $1 |
awk '/^INSERT INTO / && $3 != "NextId" && $3 != "Version" { print $0 }' |
sed '/^INSERT INTO [a-zA-Z]* VALUES(/s/(NULL)/(0)/g ; /^INSERT INTO [a-zA-Z]* VALUES(/s/(NULL,/(0,/g ; /^INSERT INTO [a-zA-Z]* VALUES(/s/,NULL,/,0,/g ; /^INSERT INTO [a-zA-Z]* VALUES(/s/,NULL,/,0,/g ; /^INSERT INTO [a-zA-Z]* VALUES(/s/,NULL)/,0)/g'
