<?php

/**
 *
 * bareos-webui - Bareos Web-Frontend
 *
 * @link      https://github.com/bareos/bareos for the canonical source repository
 * @copyright Copyright (c) 2013-2019 Bareos GmbH & Co. KG (http://www.bareos.org/)
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

$file = __DIR__ . '/commands.csv';
$csv = array_map('str_getcsv', file($file));

$arr = array();

// create module list
for($i=1; $i < count($csv[0]); $i++) {
  $arr[$csv[0][$i]] = array();
  $arr[$csv[0][$i]]['mandatory'] = array();
  $arr[$csv[0][$i]]['optional'] = array();
  // add commands and their requirement to each module
  for($j=1; $j < count($csv) - 1; $j++) {
    // push commands to mandatory or optional array
    if ( $csv[$j][$i] === "required" ) {
      array_push($arr[$csv[0][$i]]['mandatory'], $csv[$j][0]);
    } elseif ( $csv[$j][$i] === "optional" ) {
      array_push($arr[$csv[0][$i]]['optional'], $csv[$j][0]);
    }
  }
}

return array (
  'console_commands' => $arr
);
