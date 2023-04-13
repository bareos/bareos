/**
 *
 * bareos-webui - Bareos Web-Frontend
 *
 * @link      https://github.com/bareos/bareos for the canonical source repository
 * @copyright Copyright (c) 2020-2023 Bareos GmbH & Co. KG (http://www.bareos.org/)
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

function formatJobId(value, basePath) {
   return '<a href="' + basePath + '/job/details/' + value + '" title="' + iJS._("View Job Details") + '">' + value + '</a>';
}

function formatJobName(value, basePath, displayRange) {
   if(displayRange === undefined) {
      return '<a href="' + basePath + '/job/index?jobname=' + value + '">' + value + '</a>';
   } else {
      return '<a href="' + basePath + '/job/index?jobname=' + value + '&period=' + displayRange + '">' + value + '</a>';
   }
}

function formatClientName(value, basePath) {
   return '<a href="' + basePath + '/client/details?client=' + value + '">' + value + '</a>';
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
   if( Math.floor(data / 31536000) >= 1 ) {
      return Math.floor(data / 31536000) + ' ' + iJS._('year(s)');
   }
   else if( Math.floor(data / 2592000) >= 1 ) {
      return Math.floor(data / 2592000) + ' ' + iJS._('month(s)');
   }
   else if( Math.floor(data / 86400) >= 1 ) {
      return Math.floor(data / 86400) + ' ' + iJS._('day(s)');
   }
   else if( Math.floor(data / 3600) >= 1 ) {
      return Math.floor(data / 3600) + ' ' + iJS._('hour(s)');
   }
   else if( Math.floor(data / 60) >= 1 ) {
      return Math.floor(data / 60) + ' ' + iJS._('second(s)');
   }
   else {
      return '-';
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

function formatInchanger(data) {
   var r;
   if(data == 1) {
      r = '<span class="label label-success">' + iJS._("Yes") + '</span>';
   }
   else {
      r = '<span class="label label-default">' + iJS._("No") + '</span>';
   }
   return r;
}

function formatJobStatus(data) {
   var output;
   switch(data) {
      // Non-fatal error
      case 'e':
         jobstatus_e = iJS._("Non-fatal error");
         output = '<span class="label label-danger" title="' + jobstatus_e + '">' + iJS._("Failure") + '</span>';
         break;
      // Terminated with errors
      case 'E':
         jobstatus_E = iJS._("Job terminated in error");
         output = '<span class="label label-danger" title="' + jobstatus_E + '">' + iJS._("Failure") + '</span>';
         break;
      // Fatal error
      case 'f':
         jobstatus_f = iJS._("Fatal error");
         output = '<span class="label label-danger" title="' + jobstatus_f + '">' + iJS._("Failure") + '</span>';
         break;
      // Terminated successful
      case 'T':
         jobstatus_T = iJS._("Terminated normally");
         output = '<span class="label label-success" title="' + jobstatus_T + '">' + iJS._("Success") + '</span>';
         break;
      // Running
      case 'R':
         jobstatus_R = iJS._("Running");
         output = '<span class="label label-info" title="' + jobstatus_R + '">' + iJS._("Running") + '</span>';
         break;
      // Created no yet running
      case 'C':
         jobstatus_C = iJS._("Created but not yet running");
         output = '<span class="label label-default" title="' + jobstatus_C + '">' + iJS._("Waiting") + '</span>';
         break;
      // Blocked
      case 'B':
         jobstatus_B = iJS._("Blocked");
         output = '<span class="label label-warning" title="' + jobstatus_B + '">' + iJS._("Blocked") + '</span>';
         break;
      // Verify found differences
      case 'D':
         jobstatus_D = iJS._("Verify differences");
         output = '<span class="label label-warning" title="' + jobstatus_D + '">' + iJS._("Verify found differences") + '</span>';
         break;
      // Canceled by user
      case 'A':
         jobstatus_A = iJS._("Canceled by user");
         output = '<span class="label label-warning" title="' + jobstatus_A + '">' + iJS._("Canceled") + '</span>';
         break;
      // Waiting for client
      case 'F':
         jobstatus_F = iJS._("Waiting on File daemon");
         output = '<span class="label label-default" title="' + jobstatus_F + '">' + iJS._("Waiting") + '</span>';
         break;
      // Waiting for storage daemon
      case 'S':
         jobstatus_S = iJS._("Waiting on the Storage daemon");
         output = '<span class="label label-default" title="' + jobstatus_S + '">' + iJS._("Waiting") + '</span>';
         break;
      // Waiting for new media
      case 'm':
         jobstatus_m = iJS._("Waiting for new media");
         output = '<span class="label label-default" title="' + jobstatus_m + '">' + iJS._("Waiting") + '</span>';
         break;
      // Waiting for media mount
      case 'M':
         jobstatus_M = iJS._("Waiting for Mount");
         output = '<span class="label label-default" title="' + jobstatus_M + '">' + iJS._("Waiting") + '</span>';
         break;
      // Waiting for storage resource
      case 's':
         jobstatus_s = iJS._("Waiting for storage resource");
         output = '<span class="label label-default" title="' + jobstatus_s + '">' + iJS._("Waiting") + '</span>';
         break;
      // Waiting for job resource
      case 'j':
         jobstatus_j = iJS._("Waiting for job resource");
         output = '<span class="label label-default" title="' + jobstatus_j + '">' + iJS._("Waiting") + '</span>';
         break;
      // Waiting for client resource
      case 'c':
         jobstatus_c = iJS._("Waiting for Client resource");
         output = '<span class="label label-default" title="' + jobstatus_c + '">' + iJS._("Waiting") + '</span>';
         break;
      // Waiting on maximum jobs
      case 'd':
         jobstatus_d = iJS._("Waiting for maximum jobs");
         output = '<span class="label label-default" title="' + jobstatus_d + '">' + iJS._("Waiting") + '</span>';
         break;
      // Waiting on starttime
      case 't':
         jobstatus_t = iJS._("Waiting for start time");
         output = '<span class="label label-default" title="' + jobstatus_t + '">' + iJS._("Waiting") + '</span>';
         break;
      // Waiting on higher priority jobs
      case 'p':
         jobstatus_p = iJS._("Waiting for higher priority jobs to finish");
         output = '<span class="label label-default" title="' + jobstatus_p + '">' + iJS._("Waiting") + '</span>';
         break;
      // SD despooling attributes
      case 'a':
         jobstatus_a = iJS._("SD despooling attributes");
         output = '<span class="label label-info" title="' + jobstatus_a + '">' + iJS._("Running") + '</span>';
         break;
      // Doing batch insert file records
      case 'i':
         jobstatus_i = iJS._("Doing batch insert file records");
         output = '<span class="label label-info" title="' + jobstatus_i + '">' + iJS._("Running") + '</span>';
         break;
      // Incomplete
      case 'I':
         jobstatus_I = iJS._("Incomplete Job");
         output = '<span class="label label-primary" title="' + jobstatus_I + '">' + iJS._("Incomplete") + '</span>';
         break;
      // Committing data
      case 'L':
         jobstatus_L = iJS._("Committing data (last despool)");
         output = '<span class="label label-info" title="' + jobstatus_L + '">' + iJS._("Running") + '</span>';
         break;
      // Terminated with warnings
      case 'W':
         jobstatus_W = iJS._("Terminated normally with warnings");
         output = '<span class="label label-warning" title="' + jobstatus_W + '">' + iJS._("Warning") + '</span>';
         break;
      // Doing data despooling
      case 'l':
         jobstatus_l = iJS._("Doing data despooling");
         output = '<span class="label label-info" title="' + jobstatus_l +'">' + iJS._("Running") + '</span>';
         break;
      // Queued waiting for device
      case 'q':
         jobstatus_q = iJS._("Queued waiting for device");
         output = '<span class="label label-default" title="' + jobstatus_q + '">' + iJS._("Waiting") + '</span>';
         break;
      default:
         output = '<span class="label label-primary">' + data + '</span>';
         break;
   }
   return output;
}

function formatEnabledDisabledStatus(value) {
   if(value) {
      return '<span class="label label-success">' + iJS._("Enabled") + '</span>';
   } else {
      return '<span class="label label-danger">' + iJS._("Disabled") + '</span>';
   }
}

function formatUname(value, basePath) {
   let osImage = null;

   if(value.toLowerCase().search("suse") > -1) {
      osImage = "suse.png";
   }
   else if(value.toLowerCase().search("sle") > -1) {
      osImage = "suse.png";
   }
   else if(value.toLowerCase().search("debian") > -1) {
      osImage = "debian.png";
   }
   else if(value.toLowerCase().search("fedora") > -1) {
      osImage = "fedora.png";
   }
   else if(value.toLowerCase().search("centos") > -1) {
      osImage = "centos.png";
   }
   else if(value.toLowerCase().search("redhat") > -1) {
      osImage = "redhat.png";
   }
   else if(value.toLowerCase().search("ubuntu") > -1) {
      osImage = "ubuntu.png";
   }
   else if(value.toLowerCase().search("univention") > -1) {
      osImage = "univention.png";
   }
   else if(value.toLowerCase().search("win") > -1) {
      osImage = "windows.png";
   }
   else if(value.toLowerCase().search("macos") > -1) {
      osImage = "macos.png";
   }
   else if(value.toLowerCase().search("solaris") > -1) {
      osImage = "sunsolaris.png";
   }
   else if(value.toLowerCase().search("freebsd") > -1) {
      osImage = "freebsd.png";
   }

   if(osImage !== null) {
      return '<img src="' + basePath + '/img/icons/os/' + osImage + '" id="icon-os" title="' + value + '" data-toggle="tooltip" data-placement="top">';
   } else {
      return '';
   }
}

function formatScheduleName(value, basePath) {
   return '<a href="' + basePath + '/schedule/details?schedule=' + value + '">' + value + '</a>';
}

function formatFilesetName(value, row, index, basePath) {
   return '<a href="' + basePath + '/fileset/details/' + row.filesetid + '">' + row.fileset + '</a>';
}

function wrapKeywordDanger(keyword) {
  return `<span class="bg-danger text-danger">${keyword}</span>`
}

function wrapKeywordWarning(keyword) {
  return `<span class="bg-warning text-warning">${keyword}</span>`
}

function wrapJobId(jobid) {
  return `<a class="bg-info text-info" href="` + basePath + `/job/details/${jobid}">JobId=${jobid}</a>`
}

function keywordHighlight(msg) {

   const danger_keywords = ["error", "err", "cannot"];
   const warning_keywords = ["warning"];

   danger_keywords.forEach(keyword => {
      let regex = new RegExp("\\b"+keyword+"(s?)\\b", "gi")
      msg = msg.replace(regex, wrapKeywordDanger('$&'))
   })

   warning_keywords.forEach(keyword => {
      let regex = new RegExp("\\b"+keyword+"(s?)\\b", "gi")
      msg = msg.replace(regex, wrapKeywordWarning('$&'))
   })

   return msg;
}

function formatLogMessage(msg) {

   let m = (msg).replace(/\n/g, "<br />");

   m = keywordHighlight(m);
   m = m.replace(/jobid=([0-9]*)/gi, wrapJobId('$1'));

   return m;
}

function formatAutochangerStatus(value) {
   let a = '<span class="label label-default">' + iJS._('No') + '</span>';
   if(value == "1") {
      a = '<span class="label label-success">' + iJS._('Yes') + '</span>';
   }
   return a;
}

function formatPoolName(value, basePath) {
   return '<a href="' + basePath + '/pool/details/?pool=' + value + '">' + value + '</a>';
}

function formatVolumeName(value, basePath) {
   return '<a href="' + basePath + '/media/details/?volume=' + value + '">' + value + '</a>';
}

function clientsActionButtonsFormatter(value, row, index, basePath) {
   let restoreButton = '<a class="btn btn-default btn-xs" data-toggle="tooltip" data-placement="top" href="' + basePath + '/restore/index?client=' + row.name + '" title="' + iJS._("Restore") + '" id="btn-1"><span class="glyphicon glyphicon-import"></span></a>';
   let statusClientButton = '<a class="btn btn-default btn-xs" data-toggle="tooltip" data-placement="top" href="' + basePath + '/client/status?client=' + row.name + '" title="' + iJS._("Status") + '" id="btn-1"><span class="glyphicon glyphicon-search"></span></a>';
   let disableButton = '<a class="btn btn-default btn-xs" data-toggle="tooltip" data-placement="top" href="' + basePath + '/client/index?action=disable&client=' + row.name + '" title="' + iJS._("Disable") + '" id="btn-1"><span class="glyphicon glyphicon-remove"></span></a>';
   let enableButton = '<a class="btn btn-default btn-xs" data-toggle="tooltip" data-placement="top" href="' + basePath + '/client/index?action=enable&client=' + row.name + '" title="' + iJS._("Enable") + '" id="btn-1"><span class="glyphicon glyphicon-ok"></span></a>';

   if(row.enabled) {
      return restoreButton + '&nbsp;' + statusClientButton + '&nbsp;' + disableButton;
   } else {
      return restoreButton + '&nbsp;' + statusClientButton + '&nbsp;' + enableButton;
   }
}

function clientActionButtonsFormatter(value, row, index, basePath) {
   let clientRestoreButton = '<a href="' + basePath + '/restore/index?client=' + row.name + '"><button type="button" class="btn btn-default btn-xs" id="btn-1" data-toggle="tooltip" data-placement="top" title="' + iJS._("Restore") + '"><span class="glyphicon glyphicon-import"></span></button></a>';
   let clientStatusButton = '<a class="btn btn-default btn-xs" data-toggle="tooltip" data-placement="top" href="' + basePath + '/client/status?client=' + row.name + '" title="' + iJS._("Status") + '" id="btn-1"><span class="glyphicon glyphicon-search"></span></a>';

   return clientRestoreButton + '&nbsp;' + clientStatusButton;
}

function scheduleActionButtonsFormatter(value, row, index, basePath) {
   let disableButton = '<a class="btn btn-default btn-xs" data-toggle="tooltip" data-placement="top" href="' + basePath + '/schedule/index?action=disable&schedule=' + row.name + '" title="' + iJS._("Disable") + '" id="btn-1"><span class="glyphicon glyphicon-remove"></span></a>';
   let enableButton = '<a class="btn btn-default btn-xs" data-toggle="tooltip" data-placement="top" href="' + basePath + '/schedule/index?action=enable&schedule=' + row.name + '" title="' + iJS._("Enable") + '" id="btn-1"><span class="glyphicon glyphicon-ok"></span></a>';

   if(row.enabled) {
      return disableButton;
   } else {
      return enableButton;
   }
}

function jobActionButtonsFormatter(value, row, index, basePath) {
   let jobDetailsButton = '<a class="btn btn-default btn-xs" data-toggle="tooltip" data-placement="top" href="' + basePath + '/job/details/' + row.jobid + '" title="' + iJS._("View Job Details") + '" id="btn-0"><span class="glyphicon glyphicon-search"></span></a>';
   let jobRerunButton = '<a class="btn btn-default btn-xs" data-toggle="tooltip" data-placement="top" href="' + basePath + '/job/index?action=rerun&jobid=' + row.jobid + '" title="' + iJS._('Rerun') + '" id="btn-1" onclick="return confirm(\'Rerun Job ID ' + row.jobid + '?\')"><span class="glyphicon glyphicon-repeat"></span></a>';
   let jobRestoreButton = '<a class="btn btn-default btn-xs" data-toggle="tooltip" data-placement="top" href="' + basePath + '/restore/?mergefilesets=0&mergejobs=0&client=' + row.client + '&jobid=' + row.jobid + '" title="' + iJS._("Restore") + '" id="btn-1"><span class="glyphicon glyphicon-import"></span></a>';
   let jobCancelButton = '<a class="btn btn-default btn-xs" data-toggle="tooltip" data-placement="top" href="' + basePath + '/job/cancel/' + row.jobid + '" title="' + iJS._("Cancel") + '" id="btn-1" onclick="return confirm(\'Cancel Job ID ' + row.jobid + '?\')"><span class="glyphicon glyphicon-remove"></span></a>';

   switch(row.jobstatus) {
      case 'T':
      case 'W':
         switch(row.type) {
            case 'B':
               return jobDetailsButton + '&nbsp;' + jobRerunButton + '&nbsp;' + jobRestoreButton;
            case 'C':
               return jobDetailsButton + '&nbsp;' + jobRestoreButton;
            default:
               return jobDetailsButton;
         }
         break;
      case 'E':
      case 'e':
      case 'f':
      case 'A':
         switch(row.type) {
            case 'B':
               return jobDetailsButton + '&nbsp;' + jobRerunButton;
            default:
               return jobDetailsButton;
         }
         break;
      case 'F':
      case 'S':
      case 'm':
      case 'M':
      case 's':
      case 'j':
      case 'c':
      case 'd':
      case 't':
      case 'p':
      case 'q':
      case 'C':
      case 'R':
      case 'l':
         switch(row.type) {
            case 'R':
               switch(row.jobstatus) {
                  case 'R':
                  case 'l':
                     return jobDetailsButton + '&nbsp;' + jobCancelButton;
                  default:
                     return jobDetailsButton;
               }
               break;
            case 'B':
            case 'C':
               return jobDetailsButton + '&nbsp;' + jobCancelButton;
            default:
               return jobDetailsButton;
         }
         break;
      default:
         return jobDetailsButton;
   }
}

function jobResourceActionButtonsFormatter(value, row, index, basePath) {
   let runJobButton = '<a class="btn btn-default btn-xs" data-toggle="tooltip" data-placement="top" href="' + basePath + '/job/actions?action=queue&job=' + row.name + '" title="' + iJS._("Run") + '" id="btn-1"><span class="glyphicon glyphicon-play"></span></a>';
   let enableJobButton = '<a class="btn btn-default btn-xs" data-toggle="tooltip" data-placement="top" href="' + basePath + '/job/actions?action=enable&job=' + row.name + '" title="' + iJS._("Enable") + '" id="btn-1"><span class="glyphicon glyphicon-ok"></span></a>';
   let disableJobButton = '<a class="btn btn-default btn-xs" data-toggle="tooltip" data-placement="top" href="' + basePath + '/job/actions?action=disable&job=' + row.name + '" title="' + iJS._("Disable") + '" id="btn-1"><span class="glyphicon glyphicon-remove"></span></a>';

   if(row.enabled) {
      return runJobButton + '&nbsp;' + disableJobButton;
   } else {
      return runJobButton + '&nbsp;' + enableJobButton;
   }
}

function storageResourceActionButtonsFormatter(value, row, index, basePath) {
   let storageStatusButton = '<a class="btn btn-default btn-xs" data-toggle="tooltip" data-placement="top" href="' + basePath + '/storage/status?storage=' + row.name + '" title="' + iJS._("Status") + '" id="btn-1"><span class="glyphicon glyphicon-search"></span></a>';
   let manageAutochangerButton = '<a class="btn btn-default btn-xs" data-toggle="tooltip" data-placement="top" href="' + basePath + '/storage/details/' + row.name + '" title="' + iJS._("Manage autochanger") + '" id="btn-1"><span class="glyphicon glyphicon-list-alt"></span></a>';

   if(row.autochanger == "1") {
      return storageStatusButton + '&nbsp;' + manageAutochangerButton;
   } else {
      return storageStatusButton;
   }
}
