#!/bin/bash
grep "^#define VERSION" ../../../../../core/src/include/version.h | cut -b 17-  | sed 's/"//g'
