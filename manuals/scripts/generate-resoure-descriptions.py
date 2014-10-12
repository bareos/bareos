#!/usr/bin/env python

import argparse
import logging
import json
from   pprint import pprint
import re
import sys

class BareosConfigurationSchema:
    def __init__( self, json ):
        self.json = json

    def getResources(self):
        return sorted(filter( None, self.json.keys()) )

    def getResource(self, resourcename):
        return self.json[resourcename]

    def getResourceDirectives(self, resourcename):
        return sorted(filter( None, self.getResource(resourcename).keys()) )

    def getResourceDirective(self, resourcename, directive, deprecated=None):
        # TODO:
        # deprecated:
        #   None:  include deprecated
        #   False: exclude deprecated
        #   True:  only deprecated
        return BareosConfigurationSchemaDirective( self.json[resourcename][directive] )

class BareosConfigurationSchemaDirective(dict):
    def getDefaultValue( self ):
        default=None
        if dict.get( self, 'default_value' ):
            if dict.get( self, 'default_value' ) == "true":
                default="yes"
            elif dict.get( self, 'default_value' ) == "false":
                default="no"
            else:
                default=dict.get( self, 'default_value' )
        return default

    def get( self, key ):
        if key == "default_value" or key == "default":
            return self.getDefaultValue()
        else:
            return dict.get(self,key)

class BareosConfigurationSchema2Latex:
    def __init__( self, json ):
        self.json = json
        self.schema = BareosConfigurationSchema( json )

    def open(self, filename = None, mode = 'r'):
        if filename:
            self.out=open( filename, mode )
        else:
            self.out=sys.stdout

    def close(self):
        if self.out != sys.stdout:
            self.out.close()

    def getResources(self):
        result = "\\begin{itemize}\n"
        for i in self.schema.getResources():
            if i:
                result += "  \\item " + i + "\n"
        result += "\\end{itemize}\n"
        return result

    def getStringsWithModifiers(self, text, strings):
        strings['text']=strings[text]
        if text:
            if strings.get('mo'):
                return "%(mo)s%(text)s%(mc)s" % ( strings )
            else:
                return "%(text)s" % ( strings )
        else:
            return ""

    def convertCamelCase2Spaces( self, valueCC ):
        s1 = re.sub('([a-z0-9])([A-Z])', r'\1 \2', valueCC)
        #return s1
        result=[]
        for token in s1.split(' '):
            u = token.upper()
            if u in [ "ACL", "CA", "CN", "DB", "DH", "FD", "NDMP", "SD", "TLS", "VSS" ]:
                token=u
            result.append(token)
        return " ".join( result )

    def getLatexDatatypeRef( self, datatype ):
        DataType="".join([x.lower().capitalize() for x in datatype.split('_')])
        return "\\dt{%(DataType)s}" % { 'DataType': DataType }

    def getResourceDirectivesTable(self, resourcename):
        result="\\begin{center}\n"
        result+="\\begin{tabular}{l | l | l | l}\n"
        result+="%(name)-50s & %(type)-30s & %(default)-20s & %(remark)s \\\\ \n" % { 'name': "name", 'type': "type of data", 'default': "default value", 'remark': "remark" }
        result+="\\hline \n"
        for directive in self.schema.getResourceDirectives(resourcename):
            data=self.schema.getResourceDirective(resourcename, directive)

            strings={
                    'directive': self.convertCamelCase2Spaces( directive ),
                    'mc': "}",
                    'extra': [],
                    'default': "",
            }

            strings['directive_link'] = "\\linkResourceDirective{%(daemon)s}{%(resource)s}{%(directive)s}" % { 'daemon': "Dir", 'resource': resourcename, 'directive': self.convertCamelCase2Spaces( directive ) }

            strings["datatype"] = self.getLatexDatatypeRef( data['datatype'] )
            if data.get( 'equals' ):
                strings["datatype"]="= %(datatype)s" % { 'datatype': strings["datatype"] }
            else:
                strings["datatype"]="\{ %(datatype)s \}" % { 'datatype': strings["datatype"] }

            extra=[]
            if data.get( 'alias' ):
                extra.append("alias")
                strings["mo"]="\\textit{"
            if data.get( 'deprecated' ):
                extra.append("deprecated")
                strings["mo"]="\\textit{"
            if data.get( 'required' ):
                extra.append("required")
                strings["mo"]="\\textbf{"
            strings["extra"]=", ".join(extra)

            if data.get( 'default_value' ):
                strings["default"]=data.get( 'default_value' )
                if data.get( 'platform_specific' ):
                    strings["default"]+=" \\textit{\\small(platform specific)}"

            strings['define']="\\csgdef{resourceDirectiveDefined%(daemon)s%(resource)s%(directive)s}{yes}" % { 'daemon': "Dir", 'resource': resourcename, 'directive': self.convertCamelCase2Spaces( directive ) }
            strings["index"]="\\index[dir]{Directive!%(directive)s}" % ( strings )
            strings["t_directive"] = self.getStringsWithModifiers( "directive_link", strings )
            strings["t_datatype"] = self.getStringsWithModifiers( "datatype", strings )
            strings["t_default"] = self.getStringsWithModifiers( "default", strings )
            strings["t_extra"] = self.getStringsWithModifiers( "extra", strings )

            result+="%(define)-80s\n" % ( strings )
            result+="%(index)-80s\n" % ( strings )
            result+="%(t_directive)-80s &\n" % ( strings )
            result+="%(t_datatype)-80s &\n" % ( strings )
            result+="%(t_default)-80s &\n" % ( strings )
            result+="%(t_extra)s\n" % ( strings )
            result+="\\\\ \n\n" % ( strings )

        result+="\\end{tabular}\n"
        result+="\\end{center}\n"
        result+="\n"
        return result

    def writeResourceDirectivesTable(self, resourcename, filename=None):
        self.open(filename, "w")
        self.out.write( self.getResourceDirectivesTable( resourcename ) )
        self.close()

    def getResourceDirectives(self, resourcename):
        result="\\begin{description}\n\n"
        for directive in self.schema.getResourceDirectives(resourcename):
            data=self.schema.getResourceDirective(resourcename, directive)

            strings={
                'daemon': "Dir",
                'resource': resourcename,
                'directive': self.convertCamelCase2Spaces( directive ),
                'datatype': self.getLatexDatatypeRef( data['datatype'] ),
                'default': "",
                'deprecated': "",
                'required': '',
            }

            deprecated=""
            if data.get( 'deprecated' ):
                strings['deprecated']="deprecated"
            if data.get( 'required' ):
                strings['required']="required"
            if data.get( 'default_value' ):
                strings['default']=data.get( 'default_value' )

            result+="\\resourceDirective{%(daemon)s}{%(resource)s}{%(directive)s}{%(datatype)s}{%(required)s}{%(default)s}{%(deprecated)s}{}\n\n" % ( strings )
        result+="\\end{description}\n\n"
        return result

    def writeResourceDirectives(self, resourcename, filename=None):
        self.open(filename, "w")
        self.out.write( self.getResourceDirectives( resourcename ) )
        self.close()

if __name__ == '__main__':
    logging.basicConfig(format='%(levelname)s %(module)s.%(funcName)s: %(message)s', level=logging.INFO)
    logger = logging.getLogger()

    parser = argparse.ArgumentParser()
    parser.add_argument( '-d', '--debug', action='store_true', help="enable debugging output" )
    parser.add_argument("filename", help="load json file")
    args = parser.parse_args()
    if args.debug:
        logger.setLevel(logging.DEBUG)

    with open(args.filename) as data_file:
        data = json.load(data_file)
    #pprint(data)

    #print data.keys()

    schema = BareosConfigurationSchema( data )
    latex = BareosConfigurationSchema2Latex( data )
    #print schema.getResources()
    for resource in schema.getResources():
        logger.info( "resource: " + resource )
        latex.writeResourceDirectives(resource, "autogenerated/director-resource-"+resource.lower()+"-description.tex")
        latex.writeResourceDirectivesTable(resource, "autogenerated/director-resource-"+resource.lower()+"-table.tex")
