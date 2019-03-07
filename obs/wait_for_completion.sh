#!/bin/bash
OSC="osc -A https://obs2018.dass-it"

date
$OSC prjresults jenkins:master --watch --xml
date
