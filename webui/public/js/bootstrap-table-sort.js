/**
 *
 * bareos-webui - Bareos Web-Frontend
 *
 * @link      https://github.com/bareos/bareos for the canonical source repository
 * @copyright Copyright (c) 2013-2023 Bareos GmbH & Co. KG (http://www.bareos.org/)
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

function compareSegments(a, b) {
    // 1a < 1 < 1.0
    // 1abc < 1xyz < 1 < 2abc < 2
    // 1~pre1.abc < 1~pre2.abc < 1~pre10.abc
    if (a == b) {
        return 0;
    }

    if (a == undefined) {
        if (isNaN(b)) {
            return 1
        } else {
            return -1;
        }
    }

    if (b == undefined) {
        if (isNaN(a)) {
            return -1
        } else {
            return 1;
        }
    }

    if (isNaN(a) && isNaN(b)) {
        // a: str, b: str
        if (a <= b) {
            return -1;
        } else {
            return 1;
        }
    } else if (isNaN(a)) {
        // a: str, b: number
        return -1;
    } else if (isNaN(b)) {
        // a: number, b: str
        return 1;
    } else {
        // a: number, b: number
        return parseInt(a) - parseInt(b);
    }
}

function getSegments(str) {
    // split numbers and text segments. Also use '.' as separator.
    return str.match(/[\d]+|[^\d]+|\./g).filter(seg => seg != '.');
}

function sortSemanticVersion(a, b) {
    var segmentsA = getSegments(a);
    var segmentsB = getSegments(b);
    var len = Math.max(segmentsA.length, segmentsB.length);

    for (var i = 0; i < len; i++) {
        var result = compareSegments(segmentsA[i], segmentsB[i]);
        if (result != 0) {
            return result;
        }
    }
    return segmentsA.length - segmentsB.length;
}
