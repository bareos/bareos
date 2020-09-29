1) We can give the choice to the user the choose between different types of timeformats
in order to restore data before them. It is beneficial since the user could have a
vague idea of the time of the backup, or maybe the backup is automated by another
program that doesn't know precises dates, so it would be able to choose wider time frames:
Examples:
* YYYY-MM-DD : for anything before a certain date
* YYYY-MM : for anyhting before a certain month
* YYYY-MM-DD HH : for anything before a certain hour of a day

2) Default values would go to the nearest start point of each part of the date
for example, seconds, minutes and hours would default to 00 if not used, and
months and days would default to 01. The year would be the minimum requirement to
always be there.

3) * 1999-08-24 09:16 it replaces 1999-08-24 09:16:00
* 1999 : replaces 1999-01-01 00:00:00
* 1999-08 : replaces 1999-08-01 00:00:00
* 1999-08-25 : replaces 1999-08-25 00:00:00

4) the file responsible is in bareos/core/src/dird/ua_restore.cc , line 675 case number 5
There are two functions responsible for this:
* first one is to get the date from the user, it uses also some command prompt 
functions to ask for the user's input, as well as string parsing functions to 
transform a date into unix time.
* second one is to to find the backups before the selected date. This functionnality 
also calls some database handling functions in order to get stored data from a database.

5) The simplest way to implement this is to prompt the user to choose 
a date format and then enter it. After choosing the format and entering the date,
prepending default values would be added depending on the chosen format.
This should be done before the transformation into a unix time.

6) See pull request code changes.
For the testing part, I did it manually, and it worked for my part, being able
to choose the format as well as enter dates and receiving the right message (fail
message obviously since I don't have any record).

7) One bug that I found is that the seperate entities of the date are not 
checked, meaning you could put 15 as a month, but it would still get through
and the program will try to find something before that date:
for example if we put the date 1999-29-35 02:02:02 that contains a wrong day and 
month, the result gives back:

`No Full backup before 1999-29-35 02:02:02 found.`

which means the date is accepted, which is supposed to report something like 
"wrong date" for example.
