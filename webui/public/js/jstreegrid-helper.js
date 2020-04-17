/**
 *
 * bareos-webui - Bareos Web-Frontend
 *
 * @link      https://github.com/bareos/bareos for the canonical source repository
 * @copyright Copyright (c) 2020-2020 Bareos GmbH & Co. KG (http://www.bareos.org/)
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

var symbolicModeNotation = null;

var symbolicRepresentations = [
   '---', '--x', '-w-', '-wx',
   'r--', 'r-x', 'rw-', 'rwx'
];

String.prototype.replaceAt=function(index, replacement) {
    return this.substr(0, index) + replacement + this.substr(index + replacement.length);
}

function setSymbolicModeNotationFiletype(value) {
   switch(value) {
      case '1':
         // FIFO (named pipe)
         symbolicModeNotation += 'p';
         break;
      case '2':
         // character device
         symbolicModeNotation += 'c';
         break;
      case '4':
         // directory
         symbolicModeNotation += 'd';
         break;
      case '6':
         // block device
         symbolicModeNotation += 'b';
         break;
      case '10':
         // regular file
         symbolicModeNotation += '-';
         break;
      case '12':
         // symbolic link
         symbolicModeNotation += 'l';
         break;
      case '14':
         // socket
         symbolicModeNotation += 's';
         break;
      default:
         symbolicModeNotation += '-';
         break;
   }
}

function setSUID() {
   if(symbolicModeNotation.charAt(3) === '-') {
      symbolicModeNotation = symbolicModeNotation.replaceAt(3, 'S');
   } else {
      symbolicModeNotation = symbolicModeNotation.replaceAt(3, 's');
   }
}

function setSGID() {
   if(symbolicModeNotation.charAt(6) === '-') {
      symbolicModeNotation = symbolicModeNotation.replaceAt(6, 'S');
   } else {
      symbolicModeNotation = symbolicModeNotation.replaceAt(6, 's');
   }
}

function setStickyBit() {
   if(symbolicModeNotation.charAt(9) === '-') {
      symbolicModeNotation = symbolicModeNotation.replaceAt(9, 'T');
   } else {
      symbolicModeNotation = symbolicModeNotation.replaceAt(9, 't');
   }
}

function setSymbolicModeNotationPermissions(value) {
   for(var i = 1; i < value.length; i++) {
      symbolicModeNotation += symbolicRepresentations[value.charAt(i)];
   }
}

function setSymbolicModeNotationSpecialPermissions(value) {
   switch(value) {
      case '1':
         setStickyBit();
         break;
      case '2':
         setSGID();
         break;
      case '3':
         setStickyBit();
         setSGID();
         break;
      case '4':
         setSUID();
         break;
      case '5':
         setSUID();
         setStickyBit();
         break;
      case '6':
         setSUID();
         setSGID();
         break;
      case '7':
         setSUID();
         setSGID();
         setStickyBit();
         break;
      default:
         break;
   }
}

function formatJSTreeGridUserItem(node) {
   return node.data.stat.uid + ' (' + node.data.stat.user + ')';
}

function formatJSTreeGridGroupItem(node) {
   return node.data.stat.gid + ' (' + node.data.stat.group + ')';
}

function formatJSTreeGridModeItem(mode) {
   if(mode === 0) {
      return null;
   } else {
      symbolicModeNotation = "";
      let modeOctal = mode.toString(8);
      let filetypeOctal = modeOctal.slice(0, -4);
      let permissionsOctal = modeOctal.substr(modeOctal.length - 4);

      setSymbolicModeNotationFiletype(filetypeOctal);
      setSymbolicModeNotationPermissions(permissionsOctal);
      setSymbolicModeNotationSpecialPermissions(permissionsOctal.charAt(0));

      return symbolicModeNotation;
   }
}

function formatJSTreeGridSizeItem(node) {
   if(node.data.type === 'D') {
      return null;
   }
   if(node.data.stat.size === 0 && node.data.type === 'F') {
      return '0 B';
   } else {
      const units = ["B", "kiB", "MiB", "GiB", "TiB", "PiB", "EiB", "ZiB"];
      const i = Math.floor(Math.log2(node.data.stat.size) / 10);
      return parseFloat((node.data.stat.size / (1 << (i * 10))).toFixed(2)) + " " + units[i];
   }
}

function formatJSTreeGridLastModItem(mtime) {
   if(mtime === 0) {
      return null;
   } else {
      return moment.unix(mtime).format("YYYY-MM-DD HH:mm:ss");
   }
}

