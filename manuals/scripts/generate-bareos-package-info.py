#!/usr/bin/env python

# -*- coding: utf-8 -*-

"""
Generate-bareos-package-info: generate latex tables from a json data structure.

Use obs-project-binary-packages-to-json.py to create the json files.
"""

__author__ = "Joerg Steffens"
__copyright__ = "2015-2016"
__license__ = "AGPL"
__version__ = "0.2"
__email__ = "joerg.steffens@bareos.com"

import argparse
import logging
import json
import os
from   pprint import pprint
import re



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
                    result+='\\multicolumn{%i}{ c|}{%s} &\n' % (prefixcount, prefix)
                prefix = prefix_new
                prefixcount=1
            else:
                prefixcount+=1
            version = i.split('_',1)[1]
            version_header+=" & " + version.replace('_', '').replace('Leap', '').replace('SP','sp')
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


class PackageDistReleaseData:
    def __init__(self):
        self.data = {}
        self.releases = set()
        self.distributions = set()
        self.excluded_distributions = set()
        self.excluded_packages = set()
        
        self.dist_exclude_pattern = re.compile("win_cross|windows")
        self.package_exclude_pattern = re.compile("php-ZendFramework2-.*|mhvtl.*|mingw.*|.*-dbg|.*-debug.*|.*-doc|.*-devel|.*-dev")


    def generate(self, inputdata):
        # input:
        # '16.2':
        #   'packages': {
        #     package1: [ dist1, dist2, ... ],
        #     package2: [ dist1, dist2, ... ],
        #     ...
        #   },
        #   'distributions': {
        #     distribution_name: [ package1, package2, ... ],
        #     ...
        #   },
        # }

        # output:
        #   package1: {
        #     dist1: [ "15.2", "16.2" ],
        #     dist1: [ "16.2" ],
        #     ...
        #   },
        #   package2: {
        #     ...
        #   },
        
        for release in inputdata.keys():
            self.add_release(release, inputdata[release])

    def add_release(self, release, inputdata):
        self.releases.add(release)
        for package in inputdata['packages']:
            for dist in inputdata['packages'][package]:
                self.add(package, dist, release)
        
    def add(self, package, dist, release):
        
        if self.dist_exclude_pattern.match(dist):
            self.excluded_distributions.add(dist)
            return
        if self.package_exclude_pattern.match(package):
            self.excluded_packages.add(package)
            return
            
        self.distributions.add(dist)
        if package not in self.data:
            self.data[package] = {}
        if dist not in self.data[package]:
            self.data[package][dist] = []
        self.data[package][dist].append(release)
        
    def get_releases_string(self, package, dist):
        if len(self.data[package][dist]) == 1:
            return self.data[package][dist][0]
        else:
            minstr = str(min(self.data[package][dist]))
            maxstr = str(max(self.data[package][dist]))
            return minstr + "-" + maxstr
         
    def is_package_for_dists(self, package, dists):
        for dist in dists:
            if dist in self.data[package]:
                return True
        return False
 
    def get_packages_of_distributions(self, dists):
        result = {}
        for package in sorted(self.data.keys()):
            if self.is_package_for_dists(package, dists):
                result[package] = {}
                for dist in dists:
                    if dist in self.data[package]:
                        result[package][dist] = self.get_releases_string(package, dist)
        return result
        
def get_matches(inputdata, refilter):
    result = []
    for i in inputdata:
        if refilter.match(i):
            result.append(i)
    return result

def generate_overview_table(packageDistReleaseData, dist_include):
    rows = {}
    distributions = sorted(get_matches(packageDistReleaseData.distributions, dist_include))
    data = packageDistReleaseData.get_packages_of_distributions(distributions)
    for package in sorted(data.keys()):
        rows[package] = []
        for dist in distributions:
            if dist in data[package]:
                rows[package].append(data[package][dist])
            else:
                rows[package].append(' ')
    latexTable = LatexTable(distributions, rows)
    return latexTable.get()

def create_file(filename, data, filterregex):
    with open(filename, "w") as file:
        file.write(generate_overview_table(data, re.compile(filterregex)))
        file.close()
    

if __name__ == '__main__':
    
    logging.basicConfig(format='%(message)s')
    logger = logging.getLogger(__name__)
    logger.setLevel(logging.INFO)

    parser = argparse.ArgumentParser(description='Generate information about packages in distributions.')

    parser.add_argument('--debug', '-d', action='store_true', help="enable debugging output")
    parser.add_argument('--out', '-o', help="output directory", default=".")    
    parser.add_argument('filename', nargs="+", help="JSON files containing package information")

    args = parser.parse_args()

    if args.debug:
        logger.setLevel(logging.DEBUG)

    data = PackageDistReleaseData()

    for filename in args.filename:
        release = re.sub('.*bareos-([0-9.]+)-packages.json', r'\1', filename)
        with open(filename) as json_file:
            json_data = json.load(json_file)
            data.add_release(release, json_data)
 
    #print data.data

    basename = 'bareos-packages'
    dest = args.out + '/' + basename
    filenametemplate = dest + '-table-{0}.tex'
    create_file(filenametemplate.format('redhat'), data, 'CentOS.*|RHEL.*')
    create_file(filenametemplate.format('fedora'), data, 'Fedora_2.')
    create_file(filenametemplate.format('suse'), data, 'openSUSE_13..|openSUSE_Leap_.*|SLE_10_SP4|SLE_11_SP4|SLE_12_SP1')
    create_file(filenametemplate.format('debian'), data, 'Debian.*|Univention_4.*')
    create_file(filenametemplate.format('ubuntu'), data, 'xUbuntu.*')
