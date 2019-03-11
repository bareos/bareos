#!/bin/bash
OSC="osc -A https://obs2018.dass-it"

$OSC prjresults jenkins:master

date
$OSC prjresults jenkins:master --watch --xml
date

$OSC prjresults jenkins:master

# rebuild failed jobs
$OSC rebuild -f jenkins:master

date
$OSC prjresults jenkins:master --watch --xml
date

# rebuild failed jobs
$OSC rebuild -f jenkins:master

date
$OSC prjresults jenkins:master --watch --xml
date
