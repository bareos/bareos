/*
 *
 *   Nagios Plugin check_bacula
 *
 *     Christian Masopust, (c)2005
 *
 *     Version $Id: check_bacula.c,v 1.0 2005/02/25
 */

/*
   Copyright (C) 2005 Christian Masopust

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston,
   MA 02111-1307, USA.

 */

#include "bacula.h"
#include "check_bacula.h"

#define STATE_OK 0
#define STATE_WARNING 1
#define STATE_CRITICAL 2
#define STATE_UNKNOWN 3


/* Imported functions */
int authenticate_director(BSOCK *s, char *dirname, char *password);
int authenticate_file_daemon(BSOCK *s, char *fdname, char *password);
int authenticate_storage_daemon(BSOCK *s, char* sdname, char *password);

/* Forward referenced functions */
void writecmd(monitoritem* item, const char* command);
int docmd(monitoritem* item, const char* command, char *answer);

/* Static variables */
static monitoritem mitem;

/* Data received from DIR/FD/SD */
static char OKqstatus[]   = "%c000 OK .status\n";



static void usage()
{
   fprintf(stderr, _(
"Copyright (C) 2005 Christian Masopust\n"
"Written by Christian Masopust (2005)\n"
"\nVersion: " VERSION " (" BDATE ") %s %s %s\n\n"
"Usage: check_bacula [-d debug_level] -H host -D daemon -M name -P port\n"
"       -H <host>     hostname where daemon runs\n"
"       -D <daemon>   which daemon to check: dir|sd|fd\n"
"       -M <name>     name of monitor (as in bacula-*.conf)\n"
"       -K <md5-hash> password for access to daemon\n"
"       -P <port>     port where daemon listens\n"
"       -dnn          set debug level to nn\n"
"       -?            print this message.\n"
"\n"), HOST_OS, DISTNAME, DISTVER);
}


/*********************************************************************
 *
 *	   Main Bacula Tray Monitor -- User Interface Program
 *
 */
int main(int argc, char *argv[])
{
   int ch;
   DIRRES s_dird;
   CLIENT s_filed;
   STORE s_stored;

   char host[250];
   char daemon[20];
   char monitorname[100];
   char pw[200];
   int port = 0;

   char answer[1024];
   int retcode = STATE_UNKNOWN;

   unsigned int i, j;
   struct MD5Context md5c;
   unsigned char signature[16];


   struct sigaction sigignore;
   sigignore.sa_flags = 0;
   sigignore.sa_handler = SIG_IGN;
   sigfillset(&sigignore.sa_mask);
   sigaction(SIGPIPE, &sigignore, NULL);

   strcpy (pw, "");

   init_stack_dump();
   my_name_is(argc, argv, "check_bacula");
   textdomain("bacula");
   init_msg(NULL, NULL);

   while ((ch = getopt(argc, argv, "H:D:M:P:K:d:h?")) != -1) {

      switch (ch) {

		  case 'H':
		  	strcpy (host, optarg);
		  	break;

		  case 'D':
		  	strcpy (daemon, optarg);
		  	break;

		  case 'M':
		  	strcpy (monitorname, optarg);
		  	break;

		  case 'P':
		  	port = atoi(optarg);
		  	break;

		  case 'K':
		  	strcpy (pw, optarg);
		  	break;

		  case 'd':
		 	debug_level = atoi(optarg);
		 	if (debug_level <= 0) {
		 	   debug_level = 1;
		 	}
		 	break;

      	  case 'h':
      	  case '?':
      	  default:
			 usage();
			 exit(1);
      }
   }
   argc -= optind;
   //argv += optind;

   if (argc) {
      usage();
      exit(STATE_UNKNOWN);
   }

   lmgr_init_thread();

   char sig[100];
   MD5Init(&md5c);
   MD5Update(&md5c, (unsigned char *) pw, strlen(pw));
   MD5Final(signature, &md5c);
   for (i = j = 0; i < sizeof(signature); i++) {
      sprintf(&sig[j], "%02x", signature[i]);
      j += 2;
   }


   /* director ?  */
   if (strcmp (daemon, "dir") == 0) {

	   if (port != 0)
	   	 s_dird.DIRport = port;
	   else
	   	 s_dird.DIRport = 9101;

	   s_dird.address  = host;
	   s_dird.password = sig;
	   s_dird.hdr.name = monitorname;

	   mitem.type = R_DIRECTOR;
	   mitem.resource = &s_dird;
	   mitem.D_sock = NULL;

   } else if (strcmp (daemon, "sd") == 0) {

	   if (port != 0)
	   	 s_stored.SDport = port;
	   else
	   	 s_stored.SDport = 9103;

	   s_stored.address = host;
	   s_stored.password = sig;
	   s_stored.hdr.name = monitorname;

	   mitem.type = R_STORAGE;
	   mitem.resource = &s_stored;
	   mitem.D_sock = NULL;

   } else if (strcmp (daemon, "fd") == 0) {

	   if (port != 0)
	   	 s_filed.FDport = port;
	   else
	   	 s_filed.FDport = 9102;

	   s_filed.address = host;
	   s_filed.password = sig;
	   s_filed.hdr.name = monitorname;

	   mitem.type = R_CLIENT;
	   mitem.resource = &s_filed;
	   mitem.D_sock = NULL;

   } else {

	   usage();
	   exit(1);
   }


   if (mitem.type == R_DIRECTOR)
	   retcode = docmd(&mitem, ".status dir current\n", answer);
   else
	   retcode = docmd(&mitem, ".status current\n", answer);


   if (mitem.D_sock) {
	 bnet_sig(mitem.D_sock, BNET_TERMINATE); /* send EOF */
	 bnet_close(mitem.D_sock);
   }

   printf ("%s\n", answer);
   return retcode;
}


static int authenticate_daemon(monitoritem* item) {

   DIRRES *d;
   CLIENT *f;
   STORE *s;

   switch (item->type) {
   case R_DIRECTOR:
      d = (DIRRES *)item->resource;
      return authenticate_director(item->D_sock, d->hdr.name, d->password);
      break;
   case R_CLIENT:
      f = (CLIENT *)item->resource;
      return authenticate_file_daemon(item->D_sock, f->hdr.name, f->password);
      break;
   case R_STORAGE:
      s = (STORE *)item->resource;
      return authenticate_storage_daemon(item->D_sock, s->hdr.name, s->password);
      break;
   default:
      printf("Error, currentitem is not a Client or a Storage..\n");
      return FALSE;
   }
}



int docmd(monitoritem* item, const char* command, char *answer) {

   int stat;
   char num;
   const char *dname;
   
   dname = "";

   if (!item->D_sock) {

      DIRRES* dird;
      CLIENT* filed;
      STORE* stored;

      switch (item->type) {
      case R_DIRECTOR:
		 dird = (DIRRES*)item->resource;
		 item->D_sock = bnet_connect(NULL, 0, 0, 0, "Director daemon", dird->address, NULL, dird->DIRport, 0);
		 dname = "Director";
		 break;
      case R_CLIENT:
		 filed = (CLIENT*)item->resource;
		 item->D_sock = bnet_connect(NULL, 0, 0, 0, "File daemon", filed->address, NULL, filed->FDport, 0);
		 dname = "FileDaemon";
		 break;
      case R_STORAGE:
		 stored = (STORE*)item->resource;
		 item->D_sock = bnet_connect(NULL, 0, 0, 0, "Storage daemon", stored->address, NULL, stored->SDport, 0);
		 dname = "StorageDaemon";
		 break;
      default:
		 printf("Error, currentitem is not a Client, a Storage or a Director..\n");
		 return STATE_UNKNOWN;
      }

      if (item->D_sock == NULL) {
      		 sprintf (answer, "BACULA CRITICAL - Cannot connect to %s!", dname);
		 return STATE_CRITICAL;
      }

      if (!authenticate_daemon(item)) {
	 	sprintf (answer, "BACULA CRITICAL - Cannot authenticate to %s: %s", dname, item->D_sock->msg);
	 	item->D_sock = NULL;
	 	return STATE_CRITICAL;
      }

   }

   if (command[0] != 0)
      writecmd(item, command);

   while(1) {
      if ((stat = bnet_recv(item->D_sock)) >= 0) {

	/* welcome message of director */
	if ((item->type == R_DIRECTOR) && (strncmp(item->D_sock->msg, "Using ", 6) == 0))
		continue;

	if (sscanf(item->D_sock->msg, OKqstatus, &num) != 1) {
		/* Error, couldn't find OK */
		sprintf (answer, "BACULA CRITICAL - %s Status: %s", dname, item->D_sock->msg);
		return STATE_CRITICAL;
	} else {
		sprintf (answer, "BACULA OK - %s Status OK", dname);
		return STATE_OK;
	}
      }
      else if (stat == BNET_SIGNAL) {
	 	if (item->D_sock->msglen == BNET_EOD) {
	    	strcpy(answer, "BACULA WARNING - << EOD >>");
	    	return STATE_WARNING;
	 }
	 else if (item->D_sock->msglen == BNET_SUB_PROMPT) {
	    strcpy(answer, "BACULA WARNING - BNET_SUB_PROMPT signal received.");
	    return STATE_WARNING;
	 }
	 else if (item->D_sock->msglen == BNET_HEARTBEAT) {
	    bnet_sig(item->D_sock, BNET_HB_RESPONSE);
	 }
	 else {
		sprintf(answer, "BACULA WARNING - Unexpected signal received : %s ", bnet_sig_to_ascii(item->D_sock));
	 }
      }
      else { /* BNET_HARDEOF || BNET_ERROR */
		 strcpy(answer, "BACULA CRITICAL - ERROR: BNET_HARDEOF or BNET_ERROR");
		 item->D_sock = NULL;
		 return STATE_CRITICAL;
      }

      if (is_bnet_stop(item->D_sock)) {
		 item->D_sock = NULL;
		 return STATE_WARNING;
      }
   }
}

void writecmd(monitoritem* item, const char* command) {
   if (item->D_sock) {
      item->D_sock->msglen = strlen(command);
      pm_strcpy(&item->D_sock->msg, command);
      bnet_send(item->D_sock);
   }
}

