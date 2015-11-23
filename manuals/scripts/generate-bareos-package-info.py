#!/usr/bin/env python

# -*- coding: utf-8 -*-

"""ospi-client: performs operation for opsi clients on opsi server via JSON-RPC."""

__author__ = "Joerg Steffens"
__copyright__ = "2015"
__license__ = "AGPL"
__version__ = "0.1"
__email__ = "joerg.steffens@bareos.com"

import argparse
#from   debian.debfile import DebFile
import logging
import json
import os
from   pprint import pprint
import re
#import rpm

# JSON structure:
# 
# 'packages': {
#   package_name: [ dist1, dist2, ... ],
#   ...
# },
# 'distributions': {
#   distribution_name: [ package1, package2, ... ],
#   ...
# }
#

class LatexTable:
    def __init__(self, columns, rows):
        self.columns = columns
        self.rows = rows
        
    def __getHeader(self):
        result="\\begin{center}\n"
        result+="\\begin{longtable}{ l "
        result+="| c " * len(self.columns)
        result+="}\n"
        result+="\\hline \n"
        result+=self.__getHeaderColumns()
        result+="\\hline \n"
        result+="\\hline \n"
        return result
    
    def __getHeaderColumns(self):
        result=' & \n'
        version_header=''
        prefix=None
        prefixcount=0
        for i in self.columns:
            prefix_new=i.split('_',1)[0].replace('_', ' ')
            if prefix_new != prefix:
                if prefixcount:
                    #result+=" & " + prefix + str(prefixcount)
                    result+='\\multicolumn{%i}{ c|}{%s} &\n' % (prefixcount, prefix)
                prefix = prefix_new
                prefixcount=1
            else:
                prefixcount+=1
            version = i.rsplit('_',1)[1]
            version_header+=" & " + version.replace('_', '')
        if prefixcount:
            result+='\\multicolumn{%i}{ c }{%s}\n' % (prefixcount, prefix)
        result+="\\\\ \n"
        result+=version_header + "\\\\ \n"
        return result
    
    def __getRows(self):
        result=''
        for desc in sorted(self.rows.keys()):
            result+="%-40s & " % (desc)
            result+=" & ".join(self.rows[desc])
            result+=' \\\\ \n'
        return result
    
    def __getFooter(self):
        result ="\\hline \n"
        result+='\\end{longtable} \n'
        result+='\\end{center} \n'
        return result 
    
    def get(self):
        result  = self.__getHeader()
        result += self.__getRows()
        result += self.__getFooter()
        return result

#class OverviewTable:
def generate_overview_table(data, dist_include):
    dist_exclude_pattern = re.compile("win_cross|windows")
    package_exclude_pattern = re.compile("php-ZendFramework2-.*|mhvtl-kmp-.*|mingw.*|.*-debug.*")
    packages_excluded = []
    distributions = []
    for dist in sorted(data['distributions'].keys()):
        if dist_exclude_pattern.match(dist):
            logger.debug("excluded distribution %s", dist)
        else:
            if dist_include.match(dist) and data['distributions'][dist]:
                distributions.append(dist)
            else:
                logger.debug("excluded distribution %s (empty)", dist)
    logger.debug(distributions)
    rows = {}
    for package in sorted(data['packages'].keys()):
        #if not package.endswith('-debuginfo'):
        if package_exclude_pattern.match(package):
            packages_excluded.append(package)
        else:
            package_dists = data['packages'][package]
            #print "%-40s" % (package),
            rows[package] = []
            match= False
            for dist in distributions:
                if dist in package_dists:
                    #print "X",
                    rows[package].append('X')
                    match=True
                else:
                    #print " ",
                    rows[package].append(' ')
            if not match:
                del(rows[package])
            #print
    logger.debug("excluded packages: %s" % (packages_excluded))
    latexTable = LatexTable(distributions, rows)
    return latexTable.get()
                

if __name__ == '__main__':
    
    logging.basicConfig(format='%(message)s')
    logger = logging.getLogger(__name__)
    logger.setLevel(logging.INFO)

    parser = argparse.ArgumentParser(description='Generate information about packages in distributions.')

    parser.add_argument('--debug', '-d', action='store_true', help="enable debugging output")
    parser.add_argument('filename', help="JSON file containing package information")

    args = parser.parse_args()

    if args.debug:
        logger.setLevel(logging.DEBUG)
        
    with open(args.filename) as json_file:
        json_data = json.load(json_file)
    #print(json_data)

    filename = args.filename.replace('.json','-table-redhat.tex')
    with open(filename, "w") as file:
        file.write(generate_overview_table(json_data, re.compile('CentOS.*|RHEL.*|Fedora.*')))
        file.close()
    filename = args.filename.replace('.json','-table-suse.tex')
    with open(filename, "w") as file:
        file.write(generate_overview_table(json_data, re.compile('openSUSE.*|SLE.*')))
        file.close()
    filename = args.filename.replace('.json','-table-debian.tex')
    with open(filename, "w") as file:
        file.write(generate_overview_table(json_data, re.compile('Debian.*|xUbuntu.*|Univention.*')))
        file.close()
