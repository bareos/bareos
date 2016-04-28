/**
 *
 * bareos-webui - Bareos Web-Frontend
 *
 * @link      https://github.com/bareos/bareos-webui for the canonical source repository
 * @copyright Copyright (c) 2013-2015 Bareos GmbH & Co. KG (http://www.bareos.org/)
 * @license   GNU Affero General Public License (http://www.gnu.org/licenses/)
 * @author    Frank Bergkemper
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

function formatJobType(data) {
   var output;
   switch(data) {
      case 'B':
         output = 'Backup';
         break;
      case 'M':
         output = 'Migrated';
         break;
      case 'V':
         output = 'Verify';
         break;
      case 'R':
         output = 'Restore';
         break;
      case 'U':
         output = 'Console program';
         break;
      case 'I':
         output = 'Internal system job';
         break;
      case 'D':
         output = 'Admin';
         break;
      case 'A':
         output = 'Archive';
         break;
      case 'C':
         output = 'Copy of a job';
         break;
      case 'c':
         output = 'Copy job';
         break;
      case 'g':
         output = 'Migration job';
         break;
      case 'S':
         output = 'Scan';
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
         return 'Full';
      case 'D':
         return 'Differential';
      case 'I':
         return 'Incremental';
      case 'f':
         return 'VirtualFull';
      case 'B':
         return 'Base';
      case 'C':
         return 'Catalog';
      case 'V':
         return 'InitCatalog';
      case 'O':
         return 'VolumeToCatalog';
      case 'd':
         return 'DiskToCatalog';
      case 'A':
         return 'Data';
      case ' ':
         return 'None';
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
      return retention + ' day(s)';
   }
}

function formatExpiration(volstatus, lastwritten, volretention) {
   if(volstatus == "Used" || volstatus == "Full") {
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
            return '<span class="label label-danger">expired</span>';
         }

         if(expiration > 0 && expiration <= 1) {
            return '<span class="label label-warning">expires in 1 day</span>';
         }

         if(expiration > 1) {
            return '<span class="label label-warning">expires in ' + Math.ceil(expiration) + ' days</span>';
         }
      }
   }
   else {
      return Math.ceil((volretention / 60 / 60 / 24)) + ' day(s)';
   }
}

function formatLastWritten(data) {
   if(data == null || data == '' || data == 0) {
      return 'never';
   }
   else {
      var d = Date.now() / 1000;
      var a = data.split(" ");
      var b = new Date(a[0]).getTime() / 1000;
      var interval = Math.floor( (d - b) / (3600 * 24) );

      if(interval < 1) {
         return '<span id="lastwritten" data-toggle="tooltip" title="' + data + '">today</span>';
      }
      else if(interval <= 31 && interval >= 1) {
         return '<span id="lastwritten" data-toggle="tooltip" title="' + data + '">' + interval + ' day(s) ago</span>';
      }
      else if(interval >= 31 && interval <= 365) {
         return '<span id="lastwritten" data-toggle="tooltip" title="' + data + '">' + Math.round(interval / 31) + ' month(s) ago</span>';
      }
      else if(interval > 365) {
         return '<span id="lastwritten" data-toggle="tooltip" title="' + data + '">' + Math.round(interval / 365) + ' year(s) ago</span>';
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
      var a = '<span class="label label-success">enabled</span>';
   }
   else {
      var a = '<span class="label label-danger">disabled</span>';
   }
   return a;
}

function formatRecycle(data) {
   var r;
   if(data == 1) {
      r = '<span class="label label-success">Yes</span>';
   }
   else {
      r = '<span class="label label-danger">No</span>';
   }
   return r;
}

function formatJobStatus(data) {
   var output;
   switch(data) {
      // Non-fatal error
      case 'e':
         output = '<span class="label label-danger">Failure</span>';
         break;
      // Terminated with errors
      case 'E':
         output = '<span class="label label-danger">Failure</span>';
         break;
      // Fatal error
      case 'f':
         output = '<span class="label label-danger">Failure</span>';
         break;
      // Terminated successful
      case 'T':
         output = '<span class="label label-success">Success</span>';
         break;
      // Running
      case 'R':
         output = '<span class="label label-info">Running</span>';
         break;
      // Created no yet running
      case 'C':
         output = '<span class="label label-default">Queued</span>';
         break;
      // Blocked
      case 'B':
         output = '<span class="label label-warning">Blocked</span>';
         break;
      // Verify found differences
      case 'D':
         output = '<span class="label label-warning">Verify found differences</span>';
         break;
      // Canceled by user
      case 'A':
         output = '<span class="label label-warning">Canceled</span>';
         break;
      // Waiting for client
      case 'F':
         output = '<span class="label label-default">Waiting</span>';
         break;
      // Waiting for storage daemon
      case 'S':
         output = '<span class="label label-default">Waiting</span>';
         break;
      // Waiting for new media
      case 'm':
         output = '<span class="label label-default">Waiting</span>';
         break;
      // Waiting for media mount
      case 'M':
         output = '<span class="label label-default">Waiting</span>';
         break;
      // Waiting for storage resource
      case 's':
         output = '<span class="label label-default">Waiting</span>';
         break;
      // Waiting for job resource
      case 'j':
         output = '<span class="label label-default">Waiting</span>';
         break;
      // Waiting for client resource
      case 'c':
         output = '<span class="label label-default">Waiting</span>';
         break;
      // Waiting on maximum jobs
      case 'd':
         output = '<span class="label label-default">Waiting</span>';
         break;
      // Waiting on starttime
      case 't':
         output = '<span class="label label-default">Waiting</span>';
         break;
      // Waiting on higher priority jobs
      case 'p':
         output = '<span class="label label-default">Waiting</span>';
         break;
      // SD despooling attributes
      case 'a':
         output = '<span class="label label-info">SD despooling attributes</span>';
         break;
      // Doing batch insert file records
      case 'i':
         output = '<span class="label label-info">Doing batch insert file records</span>';
         break;
      // Incomplete
      case 'I':
         output = '<span class="label label-primary">Incomplete</span>';
         break;
      // Committing data
      case 'L':
         output = '<span class="label label-info">Committing data</span>';
         break;
      // Terminated with warnings
      case 'W':
         output = '<span class="label label-warning">Warning</span>';
         break;
      // Doing data despooling
      case 'l':
         output = '<span class="label label-info">Doing data despooling</span>';
         break;
      // Queued waiting for device
      case 'q':
         output = '<span class="label label-default">Queued waiting for device</span>';
         break;
      default:
         output = '<span class="label label-primary">' + data + '</span>';
         break;
   }
   return output;
}

function getLocale(locale) {
   switch(locale) {
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
      case 'ru':
      case 'ru_RU':
         lang = 'Russian.json';
         break;
      default:
         lang = 'English.json';
   }
   return lang;
}

