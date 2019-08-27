<?php

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
