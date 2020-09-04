#!/bin/bash
PID=`cat api.pid`
kill $PID
rm api.pid
