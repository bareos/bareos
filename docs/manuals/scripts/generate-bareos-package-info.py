#!/usr/bin/env python

# -*- coding: utf-8 -*-

"""
Generate-bareos-package-info: generate latex tables from a json data structure.

Use obs-project-binary-packages-to-json.py to create the json files.
"""

__author__ = "Joerg Steffens"
__copyright__ = "2015-2019"
__license__ = "AGPL"
__version__ = "0.3"
__email__ = "joerg.steffens@bareos.com"

import argparse
import logging
import json
import os
from   pprint import pprint
import re


class Dist:
    def __init__(self, obsname):
        self.obsname = obsname
        [ self.dist, self.version ] = obsname.split('_',1)
        self.dist = self.dist.replace('xUbuntu', 'Ubuntu').replace('SLE','SLES')
        self.version = self.version.replace('_', '').replace('Leap', '').replace('SP','sp')
        if self.dist == 'Debian':
            self.version = self.version.replace('.0', '')

    def __str__(self):
        return '{} {}'.format(self.dist, self.version)

    def dist(self):
        return self.dist
    
    def version(self):
        return self.version
    
    def getList(self):
        return [ self.dist, self.version ]


class BaseTable(object):
    def __init__(self, columns, rows):
        self.columns = columns
        self.rows = rows

    def getIndexes(self):
        result = ''
        return result

    def getHeader(self):
        result = ''
        return result
    
    def getRows(self):
        result = ''
        return result
    
    def getFooter(self):
        result = ''
        return result

    def get(self):
        result  = self.getIndexes()
        result += self.getHeader()
        result += self.getRows()
        result += self.getFooter()
        return result


class LatexTable(BaseTable):

    def getIndexes(self):
        result = ''
        for i in self.columns:
            result += '\\index[general]{{Platform!{0}!{1}}}'.format(Dist(i).dist, Dist(i).version)
        return result

    def getHeader(self):
        result="\\begin{center}\n"
        result+="\\begin{longtable}{ l "
        result+="| c " * len(self.columns)
        result+="}\n"
        result+="\\hline \n"
        result+=self.getHeaderColumns()
        result+="\\hline \n"
        result+="\\hline \n"
        return result
    
    def getHeaderColumns(self):
        result=' & \n'
        version_header=''
        prefix=None
        prefixcount=0
        for i in self.columns:
            prefix_new=Dist(i).dist
            if prefix_new != prefix:
                if prefixcount:
                    result+='\\multicolumn{%i}{ c|}{%s} &\n' % (prefixcount, prefix)
                prefix = prefix_new
                prefixcount=1
            else:
                prefixcount+=1
            version_header+=" & " + Dist(i).version
        if prefixcount:
            result+='\\multicolumn{%i}{ c }{%s}\n' % (prefixcount, prefix)
        result+="\\\\ \n"
        result+=version_header + "\\\\ \n"
        return result
    
    def getRows(self):
        result=''
        for desc in sorted(self.rows.keys()):
            result+='\\package{{{:s}}} & '.format(desc)
            result+=" & ".join(self.rows[desc])
            result+=' \\\\ \n'
        return result
    
    def getFooter(self):
        result ="\\hline \n"
        result+='\\end{longtable} \n'
        result+='\\end{center} \n'
        return result 


class SphinxTable(BaseTable):

    def getIndexes(self):
        result  = '.. index::\n'
        for i in self.columns:
            result += '   single: Platform; {0}; {1}\n'.format(Dist(i).dist, Dist(i).version)
        result += '\n'
        return result

    def getHeader(self):
        result  = '.. csv-table::\n'
        result += '   :header: ""'
        result += self.getHeaderColumns()
        result += '\n\n'
        return result
    
    def getHeaderColumns(self):
        result = ''
        for i in self.columns:
            result += ', "{}"'.format(Dist(i))
        return result
    
    def getRows(self):
        result=''
        for desc in sorted(self.rows.keys()):
            result+= '   {}, '.format(desc)
            result+=", ".join(self.rows[desc])
            result+='\n'
        return result
    
    def getFooter(self):
        result = "\n"
        return result 


class PackageDistReleaseData:
    def __init__(self):
        self.data = {}
        self.releases = set()
        self.distributions = set()
        self.excluded_distributions = set()
        self.excluded_packages = set()
        
        self.dist_exclude_pattern = re.compile("win_cross|windows")
        self.package_exclude_pattern = re.compile("php5-zendframework2|php-ZendFramework2.*|mhvtl.*|mingw.*|.*-dbg|.*-debug.*|.*-doc|.*-devel|.*-dev")


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

def generate_overview_table(packageDistReleaseData, dist_include, latex):
    logger = logging.getLogger()
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

    if latex:
        logger.info("Create Latex table ...")
        table = LatexTable(distributions, rows)
    else:
        logger.info("Create Sphinx table ...")
        table = SphinxTable(distributions, rows)
        
    return table.get()
    
    

def create_file(filename, data, filterregex, latex = False):
    with open(filename, "w") as file:
        file.write(generate_overview_table(data, re.compile(filterregex), latex))
        file.close()
    

if __name__ == '__main__':
    
    logging.basicConfig(format='%(message)s')
    #logger = logging.getLogger(__name__)
    logger = logging.getLogger()
    logger.setLevel(logging.INFO)

    parser = argparse.ArgumentParser(description='Generate information about packages in distributions.')

    parser.add_argument('--debug', '-d', action='store_true', help="enable debugging output")
    parser.add_argument('--out', '-o', help="output directory", default=".") 
    parser.add_argument('--latex', action='store_true', help="Create only LaTex files." )
    parser.add_argument('--sphinx', action='store_true', help="Create only RST files for Sphinx." )    
    parser.add_argument('filename', nargs="+", help="JSON files containing package information")

    args = parser.parse_args()

    if args.debug:
        logger.setLevel(logging.DEBUG)
        logger.debug('debug')
        
    if not args.latex:
        # default is sphinx
        args.sphinx = True        

    data = PackageDistReleaseData()

    for filename in args.filename:
        release = re.sub('.*bareos-([0-9.]+)-packages.json', r'\1', filename)
        with open(filename) as json_file:
            json_data = json.load(json_file)
            data.add_release(release, json_data)
 
    #print data.data

    basename = 'bareos-packages'
    dest = args.out + '/' + basename
    
    if args.latex:
        filenametemplate = dest + '-table-{0}.tex'
    else:
        filenametemplate = dest + '-table-{0}.rst.inc'

    create_file(filenametemplate.format('redhat'), data, 'CentOS.*|RHEL.*', args.latex)
    create_file(filenametemplate.format('fedora'), data, 'Fedora_2.', args.latex)
    create_file(filenametemplate.format('opensuse'), data, 'openSUSE_13..|openSUSE_Leap_.*', args.latex)
    create_file(filenametemplate.format('suse'), data, 'SLE_10_SP4|SLE_11_SP4|SLE_12_.*|SLE_15_.*', args.latex)
    create_file(filenametemplate.format('debian'), data, 'Debian.*', args.latex)
    create_file(filenametemplate.format('ubuntu'), data, 'xUbuntu.*', args.latex)
    create_file(filenametemplate.format('univention'), data, 'Univention_4.*', args.latex)    
