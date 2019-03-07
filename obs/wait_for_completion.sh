#!/bin/bash
OSC="osc -A https://obs2018.dass-it"

while sleep 30; do
date
$OSC prjresults jenkins:master
done
