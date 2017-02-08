/**
 *
 * bareos-webui - Bareos Web-Frontend
 *
 * @link      https://github.com/bareos/bareos-webui for the canonical source repository
 * @copyright Copyright (c) 2013-2016 Bareos GmbH & Co. KG (http://www.bareos.org/)
 * @license   GNU Affero General Public License (http://www.gnu.org/licenses/)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

var dt_locale = "";
var dt_textdomain = "";

function setDtLocale(val) {
   switch(val) {
      case 'cn':
      case 'cn_CN':
         dt_locale = 'cn_CN';
         break;
      case 'en':
      case 'en_EN':
         dt_locale = 'en_EN';
         break;
      case 'es':
      case 'es_ES':
         dt_locale = 'es_ES';
         break;
      case 'fr':
      case 'fr_FR':
         dt_locale = 'fr_FR';
         break;
      case 'de':
      case 'de_DE':
         dt_locale = 'de_DE';
         break;
      case 'it':
      case 'it_IT':
         dt_locale = 'it_IT';
         break;
      case 'ru':
      case 'ru_RU':
         dt_locale = 'ru_RU';
         break;
	   case 'nl':
      case 'nl_BE':
         dt_locale = 'nl_BE';
         break;
      case 'tr':
      case 'tr_TR':
         dt_locale = 'tr_TR';
	 break;
      case 'sk':
      case 'sk_SK':
         dt_locale = 'sk_SK';
         break;        
      default:
         dt_locale = 'en_EN';
   }
   initDTLocale();
}

function setDtTextDomain(val) {
   dt_textdomain = val;
}

function initDTLocale() {
   iJS.i18n.setlocale(dt_locale);
   iJS.i18n.bindtextdomain(dt_locale, dt_textdomain, "po");
   iJS.i18n.try_load_lang();
}

function formatJobType(data) {
   var output;
   switch(data) {
      case 'B':
         output = iJS._('Backup');
         break;
      case 'M':
         output = iJS._('Migrated');
         break;
      case 'V':
         output = iJS._('Verify');
         break;
      case 'R':
         output = iJS._('Restore');
         break;
      case 'U':
         output = iJS._('Console program');
         break;
      case 'I':
         output = iJS._('Internal system job');
         break;
      case 'D':
         output = iJS._('Admin');
         break;
      case 'A':
         output = iJS._('Archive');
         break;
      case 'C':
         output = iJS._('Copy of a job');
         break;
      case 'c':
         output = iJS._('Copy job');
         break;
      case 'g':
         output = iJS._('Migration job');
         break;
      case 'S':
         output = iJS._('Scan');
         break;
      case 'O':
         output = iJS._('Consolidate');
         break;
      default:
         output = data;
         break;
   }
   return output;
}

function formatJobLevel(data) {
   switch(data) {
      case 'F':
         return iJS._('Full');
      case 'D':
         return iJS._('Differential');
      case 'I':
         return iJS._('Incremental');
      case 'f':
         return iJS._('VirtualFull');
      case 'B':
         return iJS._('Base');
      case 'C':
         return iJS._('Catalog');
      case 'V':
         return iJS._('InitCatalog');
      case 'O':
         return iJS._('VolumeToCatalog');
      case 'd':
         return iJS._('DiskToCatalog');
      case 'A':
         return iJS._('Data');
      case ' ':
         return iJS._('None');
      default:
         return data;
   }
}

function formatRetention(data) {
   var retention;
   //retention = Math.round( (data / 60 / 60 / 24) );
   retention = Math.floor((data % 31536000) / 86400);
   if(retention == 0) {
      return '-';
   }
   else {
      return retention + ' ' + iJS._('day(s)');
   }
}

function formatExpiration(volstatus, lastwritten, volretention) {
   if(volstatus == iJS._("Used") || volstatus == iJS._("Full")) {
      if(lastwritten == null || lastwritten == "") {
         return '-';
      }
      else {
         var d = Date.now() / 1000;
         var a = lastwritten.split(" ");
         var b = new Date(a[0]).getTime() / 1000;
         var interval = (d - b) / (3600 * 24);
         var retention = Math.round(volretention / 60 / 60 / 24);
         var expiration = (retention - interval).toFixed(2);
         if(expiration <= 0) {
            return '<span class="label label-danger">' + iJS._("expired") + '</span>';
         }

         if(expiration > 0 && expiration <= 1) {
            return '<span class="label label-warning">' + iJS._("expires in 1 day") + '</span>';
         }

         if(expiration > 1) {
            return '<span class="label label-warning">' + iJS._("expires in") + ' ' + Math.ceil(expiration) + ' ' + iJS._("days") + '</span>';
         }
      }
   }
   else {
      return Math.ceil((volretention / 60 / 60 / 24)) + ' ' + iJS._("day(s)");
   }
}

function formatHiddenRetExp(volstatus, lastwritten, volretention) {
   if(volstatus == iJS._("Used") || volstatus == iJS._("Full")) {
      if(lastwritten == null || lastwritten == "") {
         return 100000000000;
      }
      else {
         var d = Date.now() / 1000;
         var a = lastwritten.split(" ");
         var b = new Date(a[0]).getTime() / 1000;
         var interval = (d - b) / (3600 * 24);
         var retention = Math.round(volretention / 60 / 60 / 24);
         var expiration = (retention - interval).toFixed(2);
         return Math.ceil(expiration);
      }
   }
   else {
      // This is kind of ugly but expiration can also be a negativ value, but somehow we need to sort numerical.
      // As a workaround we move the area of unused volumes with a retention way down into the negativ by
      // prepending a large negativ number out of the scope of the usual rentention times commonly used
      // to avoid further problems.
      // TODO: Find a better sorting solution for the Retention/Expiration column.
      return Math.ceil('-1000000000000'+(volretention / 60 / 60 / 24));
   }
}

function formatLastWritten(data) {
   if(data == null || data == '' || data == 0) {
      return iJS._('never');
   }
   else {
      var d = Date.now() / 1000;
      var a = data.split(" ");
      var b = new Date(a[0]).getTime() / 1000;
      var interval = Math.floor( (d - b) / (3600 * 24) );

      if(interval < 1) {
         return '<span id="lastwritten" data-toggle="tooltip" title="' + data + '">' + iJS._("today") + '</span>';
      }
      else if(interval <= 31 && interval >= 1) {
         return '<span id="lastwritten" data-toggle="tooltip" title="' + data + '">' + interval + ' ' + iJS._("day(s) ago") + '</span>';
      }
      else if(interval >= 31 && interval <= 365) {
         return '<span id="lastwritten" data-toggle="tooltip" title="' + data + '">' + Math.round(interval / 31) + ' ' + iJS._("month(s) ago") + '</span>';
      }
      else if(interval > 365) {
         return '<span id="lastwritten" data-toggle="tooltip" title="' + data + '">' + Math.round(interval / 365) + ' ' + iJS._("year(s) ago") + '</span>';
      }
      else {
         return data;
      }
   }
}

function formatBytes(data) {
   if(data == 0 || data == null) {
      var b = "0.00 B";
   }
   else {
      var k = 1000;
      var units = ["B", "KB", "MB", "GB", "TB", "PB", "EB", "ZB"];
      var i = Math.floor(Math.log(data) / Math.log(k));
      var b = parseFloat((data / Math.pow(k, i)).toFixed(2)) + " " + units[i];
   }
   return b;
}

function formatFreeBytes(maxvolbytes, volbytes) {
   if(maxvolbytes == null || maxvolbytes == 0) return "0.00 B";
   if(volbytes == null || volbytes == 0) return "0.00 B";
   var d = maxvolbytes - volbytes;
   var k = 1000;
   var units = ["B", "KB", "MB", "GB", "TB", "PB", "EB", "ZB"];
   var i = Math.floor(Math.log(d) / Math.log(k));
   return parseFloat((d / Math.pow(k, i)).toFixed(2)) + " " + units[i];
}

function formatAutoprune(data) {
   if(data == 1) {
      var a = '<span class="label label-success">' + iJS._("enabled") + '</span>';
   }
   else {
      var a = '<span class="label label-danger">' + iJS._("disabled") + '</span>';
   }
   return a;
}

function formatRecycle(data) {
   var r;
   if(data == 1) {
      r = '<span class="label label-success">' + iJS._("Yes") + '</span>';
   }
   else {
      r = '<span class="label label-danger">' + iJS._("No") + '</span>';
   }
   return r;
}

function formatJobStatus(data) {
   var output;
   switch(data) {
      // Non-fatal error
      case 'e':
         output = '<span class="label label-danger">' + iJS._("Failure") + '</span>';
         break;
      // Terminated with errors
      case 'E':
         output = '<span class="label label-danger">' + iJS._("Failure") + '</span>';
         break;
      // Fatal error
      case 'f':
         output = '<span class="label label-danger">' + iJS._("Failure") + '</span>';
         break;
      // Terminated successful
      case 'T':
         output = '<span class="label label-success">' + iJS._("Success") + '</span>';
         break;
      // Running
      case 'R':
         output = '<span class="label label-info">' + iJS._("Running") + '</span>';
         break;
      // Created no yet running
      case 'C':
         output = '<span class="label label-default">' + iJS._("Queued") + '</span>';
         break;
      // Blocked
      case 'B':
         output = '<span class="label label-warning">' + iJS._("Blocked") + '</span>';
         break;
      // Verify found differences
      case 'D':
         output = '<span class="label label-warning">' + iJS._("Verify found differences") + '</span>';
         break;
      // Canceled by user
      case 'A':
         output = '<span class="label label-warning">' + iJS._("Canceled") + '</span>';
         break;
      // Waiting for client
      case 'F':
         output = '<span class="label label-default">' + iJS._("Waiting") + '</span>';
         break;
      // Waiting for storage daemon
      case 'S':
         output = '<span class="label label-default">' + iJS._("Waiting") + '</span>';
         break;
      // Waiting for new media
      case 'm':
         output = '<span class="label label-default">' + iJS._("Waiting") + '</span>';
         break;
      // Waiting for media mount
      case 'M':
         output = '<span class="label label-default">' + iJS._("Waiting") + '</span>';
         break;
      // Waiting for storage resource
      case 's':
         output = '<span class="label label-default">' + iJS._("Waiting") + '</span>';
         break;
      // Waiting for job resource
      case 'j':
         output = '<span class="label label-default">' + iJS._("Waiting") + '</span>';
         break;
      // Waiting for client resource
      case 'c':
         output = '<span class="label label-default">' + iJS._("Waiting") + '</span>';
         break;
      // Waiting on maximum jobs
      case 'd':
         output = '<span class="label label-default">' + iJS._("Waiting") + '</span>';
         break;
      // Waiting on starttime
      case 't':
         output = '<span class="label label-default">' + iJS._("Waiting") + '</span>';
         break;
      // Waiting on higher priority jobs
      case 'p':
         output = '<span class="label label-default">' + iJS._("Waiting") + '</span>';
         break;
      // SD despooling attributes
      case 'a':
         output = '<span class="label label-info">' + iJS._("SD despooling attributes") + '</span>';
         break;
      // Doing batch insert file records
      case 'i':
         output = '<span class="label label-info">' + iJS._("Doing batch insert file records") + '</span>';
         break;
      // Incomplete
      case 'I':
         output = '<span class="label label-primary">' + iJS._("Incomplete") + '</span>';
         break;
      // Committing data
      case 'L':
         output = '<span class="label label-info">' + iJS._("Committing data") + '</span>';
         break;
      // Terminated with warnings
      case 'W':
         output = '<span class="label label-warning">' + iJS._("Warning") + '</span>';
         break;
      // Doing data despooling
      case 'l':
         output = '<span class="label label-info">' + iJS._("Doing data despooling") + '</span>';
         break;
      // Queued waiting for device
      case 'q':
         output = '<span class="label label-default">' + iJS._("Queued waiting for device") + '</span>';
         break;
      default:
         output = '<span class="label label-primary">' + data + '</span>';
         break;
   }
   return output;
}

function getLocale(locale) {
   switch(locale) {
      case 'cn':
      case 'cn_CN':
         lang = 'Chinese.json';
         break;
      case 'en':
      case 'en_EN':
         lang = 'English.json';
         break;
      case 'fr':
      case 'fr_FR':
         lang = 'French.json';
         break;
      case 'de':
      case 'de_DE':
         lang = 'German.json';
         break;
      case 'it':
      case 'it_IT':
         lang = 'Italian.json';
         break;
      case 'ru':
      case 'ru_RU':
         lang = 'Russian.json';
         break;
      case 'es':
      case 'es_ES':
         lang = 'Spanish.json';
         break;
      case 'nl':
      case 'nl_BE':
         lang = 'Dutch.json';
         break;
      case 'tr':
      case 'tr_TR':
         lang = 'Turkish.json';
         break;
      case 'sk':
      case 'sk_SK':
         lang = 'Slovak.json';
         break;
      default:
         lang = 'English.json';
   }
   return lang;
}
