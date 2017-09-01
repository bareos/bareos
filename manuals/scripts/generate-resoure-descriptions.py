#!/usr/bin/env python

import argparse
import logging
import json
from   pprint import pprint
import re
import sys

class daemonName:
    def __init__( self ):
        table = {
            "Director": "Dir",
            "StorageDaemon": "Sd",
            "FileDaemon": "Fd"
        }

    @staticmethod
    def getLong( string ):
        if string.lower() == "director" or string.lower() == "dir":
            return "Director"
        elif string.lower() == "storagedaemon" or string.lower() == "sd":
            return "StorageDaemon"
        elif string.lower() == "filedaemon" or string.lower() == "fd":
            return "FileDaemon"

    @staticmethod
    def getShort( string ):
        if string.lower() == "bareos-dir" or string.lower() == "director" or string.lower() == "dir":
            return "Dir"
        elif string.lower() == "bareos-sd" or string.lower() == "storagedaemon" or string.lower() == "sd":
            return "Sd"
        elif string.lower() == "bareos-fd" or string.lower() == "filedaemon" or string.lower() == "fd":
            return "Fd"
        elif string.lower() == "bareos-console" or string.lower() == "bconsole" or string.lower() == "con":
            return "Console"
        elif string.lower() == "bareos-tray-monitor":
            return "Console"

class BareosConfigurationSchema:
    def __init__( self, json ):
        self.format_version_min = 2
        self.logger = logging.getLogger()
        self.json = json
        try:
            self.format_version = json['format-version']
        except KeyError as e:
            raise
        logger.info( "format-version: " + str(self.format_version) )
        if self.format_version < self.format_version_min:
            raise RuntimeError( "format-version is " + str(self.format_version) + ". Required: >= " + str(self.format_version_min) )

    def getDaemons(self):
        return sorted(filter( None, self.json["resource"].keys()))

    def getDatatypes(self):
        try:
            return sorted(filter( None, self.json["datatype"].keys()) )
        except KeyError:
            return

    def getDatatype(self, name):
        return self.json["datatype"][name]

    def getResources(self, daemon):
        return sorted(filter( None, self.json["resource"][daemon].keys()) )

    def getResource(self, daemon, resourcename):
        return self.json["resource"][daemon][resourcename]

    def getResourceDirectives(self, daemon, resourcename):
        return sorted(filter( None, self.getResource(daemon, resourcename).keys()) )

    def getResourceDirective(self, daemon, resourcename, directive, deprecated=None):
        # TODO:
        # deprecated:
        #   None:  include deprecated
        #   False: exclude deprecated
        #   True:  only deprecated
        return BareosConfigurationSchemaDirective( self.json["resource"][daemon][resourcename][directive] )

class BareosConfigurationSchemaDirective(dict):
    def getDefaultValue( self ):
        default=None
        if dict.get( self, 'default_value' ):
            if (dict.get( self, 'default_value' ) == "true") or (dict.get( self, 'default_value' ) == "on"):
                default="yes"
            elif (dict.get( self, 'default_value' ) == "false") or (dict.get( self, 'default_value' ) == "off"):
                default="no"
            else:
                default=dict.get( self, 'default_value' )
        return default

    def getStartVersion( self ):
        if dict.get( self, 'versions' ):
            version = dict.get( self, 'versions' ).partition("-")[0]
            if version:
                return version

    def getEndVersion( self ):
        if dict.get( self, 'versions' ):
            version = dict.get( self, 'versions' ).partition("-")[2]
            if version:
                return version

    def get( self, key, default=None ):
        if key == "default_value" or key == "default":
            return self.getDefaultValue()
        elif key == "start_version":
            if self.getStartVersion():
                return self.getStartVersion()
        elif key == "end_version":
            if self.getEndVersion():
                return self.getEndVersion()
        return dict.get(self, key, default)

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

    def getResources(self, daemon):
        result = "\\begin{itemize}\n"
        for i in self.schema.getResources(daemon):
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
        result=[]
        for token in s1.split(' '):
            u = token.upper()
            if u in [ "ACL", "CA", "CN", "DB", "DH", "FD", "LMDB", "NDMP", "SD", "SSL", "TLS", "VSS" ]:
                token=u
            result.append(token)
        return " ".join( result )

    def getLatexDatatypeRef( self, datatype ):
        DataType="".join([x.lower().capitalize() for x in datatype.split('_')])
        return "\\dt{%(DataType)s}" % { 'DataType': DataType }

    def getLatexDefaultValue( self, data ):
        default=""
        if data.get( 'default_value' ):
            default=data.get( 'default_value' )
            if data.get( 'platform_specific' ):
                default+=" \\textit{\\small(platform specific)}"
        return default

    def getLatexDescription(self, data):
        description = ""
        if data.get('description'):
            description = data.get('description').replace('_','\_')
        return description

    def getLatexTable(self, subtree, latexDefine="define%(key)s", latexLink="\\hyperlink{key%(key)s}{%(key)s}" ):
        result="\\begin{center}\n"
        result+="\\begin{longtable}{ l | l | l | l }\n"
        result+="\\hline \n"
        result+="\\multicolumn{1}{ c|}{\\textbf{%(name)-80s}} &\n" % { 'name': "configuration directive name" }
        result+="\\multicolumn{1}{|c|}{\\textbf{%(type)-80s}} &\n" % { 'type': "type of data" }
        result+="\\multicolumn{1}{|c|}{\\textbf{%(default)-80s}} &\n" % { 'default': "default value" }
        result+="\\multicolumn{1}{|c }{\\textbf{%(remark)-80s}} \\\\ \n" % { 'remark': "remark" }
        result+="\\hline \n"
        result+="\\hline \n"

        for key in sorted(filter( None, subtree.keys() ) ):
            data=BareosConfigurationSchemaDirective(subtree[key])

            strings={
                    'key': self.convertCamelCase2Spaces( key ),
                    'mc': "}",
                    'extra': [],
                    'default': self.getLatexDefaultValue( data ),
            }

            strings['directive_link'] = latexLink % strings

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

            define = "\\csgdef{resourceDirectiveDefined" + latexDefine + "}{yes}"
            strings['define']= define % strings
            strings["t_directive"] = self.getStringsWithModifiers( "directive_link", strings )
            strings["t_datatype"] = self.getStringsWithModifiers( "datatype", strings )
            strings["t_default"] = self.getStringsWithModifiers( "default", strings )
            strings["t_extra"] = self.getStringsWithModifiers( "extra", strings )

            result+="%(define)-80s\n" % ( strings )
            result+="%(t_directive)-80s &\n" % ( strings )
            result+="%(t_datatype)-80s &\n" % ( strings )
            result+="%(t_default)-80s &\n" % ( strings )
            result+="%(t_extra)s\n" % ( strings )
            result+="\\\\ \n\n" % ( strings )

        result+="\\hline \n"
        result+="\\end{longtable}\n"
        result+="\\end{center}\n"
        result+="\n"
        return result

    def writeResourceDirectivesTable(self, daemon, resourcename, filename=None):
        ds=daemonName.getShort(daemon)
        self.open(filename, "w")
        self.out.write( self.getLatexTable( self.schema.json["resource"][daemon][resourcename], latexDefine=ds+resourcename+"%(key)s", latexLink="\\linkResourceDirective*{"+ds+"}{"+resourcename+"}{%(key)s}" ) )
        self.close()

    def writeDatatypeOptionsTable(self, filename=None):
        self.open(filename, "w")
        self.out.write( latex.getLatexTable( schema.getDatatype( "OPTIONS" )["values"], latexDefine="DatatypeOptions%(key)s" )
        )
        self.close()

    def getResourceDirectives(self, daemon, resourcename):
        result="\\begin{description}\n\n"
        for directive in self.schema.getResourceDirectives(daemon,resourcename):
            data=self.schema.getResourceDirective(daemon,resourcename, directive)

            strings={
                'daemon': daemonName.getShort( daemon ),
                'resource': resourcename,
                'directive': self.convertCamelCase2Spaces( directive ),
                'datatype': self.getLatexDatatypeRef( data['datatype'] ),
                'default': self.getLatexDefaultValue( data ),
                'version': data.get( 'start_version', "" ),
                'description': self.getLatexDescription(data),
                'required': '',
            }

            if data.get( 'alias' ):
                if not strings['description']:
                    strings['description']="\\textit{This directive is an alias.}"
            if data.get( 'deprecated' ):
                # overwrites start_version
                strings['version']="deprecated"
            if data.get( 'required' ):
                strings['required']="required"

            result+="\\resourceDirective{%(daemon)s}{%(resource)s}{%(directive)s}{%(datatype)s}{%(required)s}{%(default)s}{%(version)s}{%(description)s}\n\n" % ( strings )
        result+="\\end{description}\n\n"
        return result

    def writeResourceDirectives(self, daemon, resourcename, filename=None):
        self.open(filename, "w")
        self.out.write( self.getResourceDirectives( daemon, resourcename ) )
        self.close()

    def getResourceDirectiveDefs(self, daemon, resourcename):
        result=""
        for directive in self.schema.getResourceDirectives(daemon, resourcename):
            data=self.schema.getResourceDirective(daemon, resourcename, directive)

            strings={
                'daemon': daemonName.getShort( daemon ),
                'resource': resourcename,
                'directive': self.convertCamelCase2Spaces( directive ),
            }

            result+="\\defDirective{%(daemon)s}{%(resource)s}{%(directive)s}{}{}{%%\n" % ( strings )
            result+="}\n\n"
        return result

    def writeResourceDirectiveDefs(self, daemon, resourcename, filename=None):
        self.open(filename, "w")
        self.out.write( self.getResourceDirectiveDefs( daemon, resourcename ) )
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

    schema = BareosConfigurationSchema( data )
    latex = BareosConfigurationSchema2Latex( data )
    #print schema.getDaemons()
    #print schema.getResources()
    for daemon in schema.getDaemons():
        #pprint(schema.getResources(daemon))
        for resource in schema.getResources(daemon):
            logger.info( "daemon: " + daemon + ", resource: " + resource )
            #pprint(schema.getResource(daemon,resource))
            latex.writeResourceDirectives(daemon, resource, "autogenerated/" + daemon.lower()+ "-resource-"+resource.lower()+"-description.tex")
            latex.writeResourceDirectiveDefs(daemon, resource, "autogenerated/" + daemon.lower()+ "-resource-"+resource.lower()+"-defDirective.tex")
            latex.writeResourceDirectivesTable(daemon, resource, "autogenerated/" + daemon.lower()+ "-resource-"+resource.lower()+"-table.tex")

    if schema.getDatatypes():
        #print schema.getDatatypes()
        #print schema.getDatatype( "OPTIONS" )
        #print latex.getLatexTable( schema.getDatatype( "OPTIONS" )["values"], latexDefine="%(key)s", latexLink="\\linkResourceDirective{%(key)s}" )
        latex.writeDatatypeOptionsTable( filename="autogenerated/datatype-options-table.tex" )
