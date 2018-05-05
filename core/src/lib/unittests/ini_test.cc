/*
   Copyright (C) 2011-2011 Bacula Systems(R) SA
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2017 Bareos GmbH & Co. KG

   This program is Free Software; you can modify it under the terms of
   version three of the GNU Affero General Public License as published by the
   Free Software Foundation, which is listed in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/
/*
 * extracted the TEST_PROGRAM functionality from the files in ..
 * and adapted for gtest
 *
 * Philipp Storz, November 2017
 */
#include <stdio.h>

int err=0;
int nb=0;
void _ok(const char *file, int l, const char *op, int value, const char *label)
{
   nb++;
   if (!value) {
      err++;
      printf("ERR %.45s %s:%i on %s\n", label, file, l, op);
   } else {
      printf("OK  %.45s\n", label);
   }
}

#define ok(x, label) _ok(__FILE__, __LINE__, #x, (x), label)

void _nok(const char *file, int l, const char *op, int value, const char *label)
{
   nb++;
   if (value) {
      err++;
      printf("ERR %.45s %s:%i on !%s\n", label, file, l, op);
   } else {
      printf("OK  %.45s\n", label);
   }
}

#define nok(x, label) _nok(__FILE__, __LINE__, #x, (x), label)

int report()
{
   printf("Result %i/%i OK\n", nb - err, nb);
   return err>0;
}

struct ini_items test_items[] = {
   /* name type comment req */
   { "datastore", INI_CFG_TYPE_NAME, "Target Datastore", 0 },
   { "newhost", INI_CFG_TYPE_STR, "New Hostname", 1 },
   { "int64val", INI_CFG_TYPE_INT64, "Int64", 1 },
   { "list", INI_CFG_TYPE_ALIST_STR, "list", 0 },
   { "bool", INI_CFG_TYPE_BOOL, "Bool", 0 },
   { "pint64", INI_CFG_TYPE_PINT64,  "pint", 0 },
   { "int32", INI_CFG_TYPE_INT32, "int 32bit", 0 },
   { "plugin.test", INI_CFG_TYPE_STR, "test with .", 0 },
   { NULL, NULL, NULL, 0 }
};

int main()
{
   FILE *fp;
   int pos;
   ConfigFile *ini = new ConfigFile();
   PoolMem *buf;

   nok(ini->RegisterItems(test_items, 5), "Check bad sizeof ini_items");
   ok(ini->RegisterItems(test_items, sizeof(struct ini_items)), "Check sizeof ini_items");

   if ((fp = fopen("test.cfg", "w")) == NULL) {
      exit (1);
   }
   fprintf(fp, "# this is a comment\ndatastore=datastore1\nnewhost=\"host1\"\n");
   fflush(fp);

   nok(ini->parse("test.cfg"), "Test missing member");
   ini->ClearItems();

   fprintf(fp, "int64val=12 # with a comment\n");
   fprintf(fp, "int64val=10 # with a comment\n");
   fprintf(fp, "int32=100\n");
   fprintf(fp, "bool=yes\n");
   fprintf(fp, "plugin.test=parameter\n");

   fflush(fp);

   ok(ini->parse("test.cfg"), "Test with all members");

   ok(ini->items[0].found, "Test presence of char[]");
   ok(!strcmp(ini->items[0].val.nameval, "datastore1"), "Test char[]");
   ok(ini->items[1].found, "Test presence of char*");
   ok(!strcmp(ini->items[1].val.strval, "host1"), "Test char*");
   ok(ini->items[2].found, "Test presence of int");
   ok(ini->items[2].val.int64val == 10, "Test int");
   ok(ini->items[4].val.boolval == true, "Test bool");
   ok(ini->items[6].val.int32val == 100, "Test int 32");

   alist *list = ini->items[3].val.alistval;
   nok(ini->items[3].found, "Test presence of alist");

   fprintf(fp, "list=a\nlist=b\nlist=c\n");
   fflush(fp);

   ini->ClearItems();
   ok(ini->parse("test.cfg"), "Test with all members");

   list = ini->items[3].val.alistval;
   ok(ini->items[3].found, "Test presence of alist");
   ok(list != NULL, "Test list member");
   ok(list->size() == 3, "Test list size");

   ok(!strcmp((char *)list->get(0), "a"), "Testing alist[0]");
   ok(!strcmp((char *)list->get(1), "b"), "Testing alist[1]");
   ok(!strcmp((char *)list->get(2), "c"), "Testing alist[2]");

   system("cp -f test.cfg test3.cfg");

   fprintf(fp, "pouet='10, 11, 12'\n");
   fprintf(fp, "pint=-100\n");
   fprintf(fp, "int64val=-100\n"); /* TODO: fix negative numbers */
   fflush(fp);

   ini->ClearItems();
   ok(ini->parse("test.cfg"), "Test with errors");
   nok(ini->items[5].found, "Test presence of positive int");

   fclose(fp);
   ini->ClearItems();
   ini->FreeItems();

   /* Test  */
   if ((fp = fopen("test2.cfg", "w")) == NULL) {
      exit (1);
   }
   fprintf(fp,
           "# this is a comment\n"
           "optprompt=\"Datastore Name\"\n"
           "datastore=@NAME@\n"
           "optprompt=\"New Hostname to create\"\n"
           "newhost=@STR@\n"
           "optprompt=\"Some 64 integer\"\n"
           "optrequired=yes\n"
           "int64val=@INT64@\n"
           "list=@ALIST@\n"
           "bool=@BOOL@\n"
           "pint64=@PINT64@\n"
           "pouet=@STR@\n"
           "int32=@INT32@\n"
           "plugin.test=@STR@\n"
      );
   fclose(fp);

   buf = new PoolMem(PM_BSOCK);
   ok(ini->UnSerialize("test2.cfg"), "Test dynamic parse");
   ok(ini->Serialize("test4.cfg"), "Try to dump the item table in a file");
   ok(ini->Serialize(buf) > 0, "Try to dump the item table in a buffer");
   ok(ini->parse("test3.cfg"), "Parse test file with dynamic grammar");

   ok((pos = ini->GetItem("datastore")) == 0, "Check datastore definition");
   ok(ini->items[pos].found, "Test presence of char[]");
   ok(!strcmp(ini->items[pos].val.nameval, "datastore1"), "Test char[]");
   ok(!strcmp(ini->items[pos].comment, "Datastore Name"), "Check comment");
   ok(ini->items[pos].required == false, "Check required");

   ok((pos = ini->GetItem("newhost")) == 1, "Check newhost definition");
   ok(ini->items[pos].found, "Test presence of char*");
   ok(!strcmp(ini->items[pos].val.strval, "host1"), "Test char*");
   ok(ini->items[pos].required == false, "Check required");

   ok((pos = ini->GetItem("int64val")) == 2, "Check int64val definition");
   ok(ini->items[pos].found, "Test presence of int");
   ok(ini->items[pos].val.int64val == 10, "Test int");
   ok(ini->items[pos].required == true, "Check required");

   ok((pos = ini->GetItem("bool")) == 4, "Check bool definition");
   ok(ini->items[pos].val.boolval == true, "Test bool");

   ok(ini->DumpResults(buf), "Test to dump results");
   printf("<%s>\n", buf);

   ini->ClearItems();
   ini->FreeItems();
   report();

   delete buf;

   exit (0);
}
