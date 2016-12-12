/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2010 Free Software Foundation Europe e.V.
   Copyright (C) 2016-2016 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/

/* Definitions of internal function codes */

/* Functions that work on current line */
#define F_CSRRGT   301                /**< cursor right */
#define F_CSRLFT   302                /**< cursor left */
#define F_ERSCHR   303                /**< erase character */
#define F_INSCHR   304                /**< insert character */
#define F_DELCHR   305                /**< delete next character character */
#define F_SOL      306                /**< go to start of line */
#define F_EOL      307                /**< go to end of line */
#define F_DELEOL   308                /**< delete to end of line */
#define F_NXTWRD   309                /**< go to next word */
#define F_PRVWRD   310                /**< go to previous word */
#define F_DELWRD   311                /**< delete word */
#define F_ERSLIN   312                /**< erase line */
#define F_TAB      313                /**< tab */
#define F_TABBAK   314                /**< tab backwards */
#define F_DELSOL   315                /**< delete from start of line to cursor */
#define F_LEFT40   316                /**< move cursor left 40 cols */
#define F_RIGHT40  317                /**< move cursor right 40 cols */
#define F_CASE     318                /**< change case of next char advance csr */
#define F_CENTERL  319                /**< center line */

/* Functions that move the cursor line or work on groups of lines */
#define F_CSRDWN   401                /**< cursor down */
#define F_CSRUP    402                /**< cursor up */
#define F_HOME     403                /**< home cursor */
#define F_EOF      404                /**< go to end of file */
#define F_PAGDWN   405                /**< page down */
#define F_PAGUP    406                /**< page up */
#define F_CENTER   407                /**< center cursor on screen */
#define F_SPLIT    408                /**< split line at cursor */
#define F_DELLIN   409                /**< delete line */
#define F_CONCAT   410                /**< concatenate next line to current */
#define F_RETURN   411                /**< carriage return */
#define F_NXTMCH   412                /**< next match */
#define F_DWN5     413                /**< cursor down 5 lines */
#define F_UP5      414                /**< cursor up 5 lines */
#define F_PUSH     415                /**< push current location */
#define F_POP      416                /**< pop previous location */
#define F_PAREN    417                /**< find matching paren */
#define F_POPVIEW  418                /**< pop to saved view */
#define F_OOPS     419                /**< restore last oops buffer */
#define F_PARENB   420                /**< find matching paren backwards */
#define F_BOTSCR   421                /**< cursor to bottom of screen */
#define F_TOPSCR   422                /**< cursor to top of screen */
#define F_TOPMARK  423                /**< cursor to top marker line */
#define F_BOTMARK  424                /**< cursor to bottom marker line */
#define F_CPYMARK  425                /**< copy marked lines */
#define F_MOVMARK  426                /**< move marked lines */
#define F_DELMARK  427                /**< delete marked lines */
#define F_SHFTLEFT 428                /**< shift marked text left one char */
#define F_SHFTRIGHT 429               /**< shift marked text right one char */

/* Miscellaneous */
#define F_ESCAPE   501                /**< escape character */
#define F_ESC      501                /**< escape character */
#define F_EOI      502                /**< end of input */
#define F_TENTRY   503                /**< toggle entry mode */
#define F_TINS     504                /**< toggle insert mode */
#define F_MARK     505                /**< set marker on lines */
#define F_CRESC    506                /**< carriage return, escape */
#define F_MACDEF   507                /**< begin "macro" definition */
#define F_MACEND   508                /**< end "macro" definition */
#define F_ZAPESC   509                /**< clear screen, escape */
#define F_CLRMARK  510                /**< clear marked text */
#define F_MARKBLK  511                /**< mark blocks */
#define F_MARKCHR  512                /**< mark characters */
#define F_HOLD     513                /**< hold line */
#define F_DUP      514                /**< duplicate line */
#define F_CHANGE   515                /**< apply last change command */
#define F_RCHANGE  516                /**< reverse last change command */
#define F_NXTFILE  517                /**< next file */
#define F_INCLUDE  518                /**< include */
#define F_FORMAT   519                /**< format paragraph */
#define F_HELP     520                /**< help */
#define F_JUSTIFY  521                /**< justify paragraph */
#define F_SAVE     522                /**< save file -- not implemented */
#define F_MOUSEI   523                /**< mouse input coming -- not completed */
#define F_SCRSIZ   524                /**< Screen size coming */
#define F_PASTECB  525                /**< Paste clipboard */
#define F_CLRSCRN  526                /**< Clear the screen */
#define F_CRNEXT   527                /**< Send line, get next line */
#define F_BREAK    528                /**< Break */
#define F_BACKGND  529                /**< go into background */
