# -*- coding: utf-8 -*-

from sphinx.domains import Domain, ObjType
from sphinx.roles import XRefRole
from sphinx.domains.std import GenericObject, StandardDomain
from sphinx.directives import ObjectDescription
from sphinx.util.nodes import clean_astext, make_refnode
from sphinx.util import logging
from sphinx.util import ws_re
from sphinx import addnodes
from sphinx.util.docfields import Field
from docutils import nodes
from pprint import pformat
import re

#
# modifies the default rule for rendering
# .. config::option:
#
# Also modifes the generated index.
# If this extension is not loaded,
# index will be 'command line option' ...
#

# phpmyadmin:
# anchor: #cfg_DirJobAlwaysIncrementalMaxFullAge
# index:  configuration option: ...

# see
# https://github.com/sphinx-doc/sphinx/blob/master/sphinx/directives/__init__.py
# https://www.sphinx-doc.org/en/master/extdev/domainapi.html

#
# Adapted for Bareos:
#
# anchor: #config-{daemon}_{resource}_{CamelCaseDirective}
          #config-Director_Job_AlwaysIncrementalMaxFullAge
# index:  Configuration Directive; {directive} ({daemon}->{resource})

# TODO
# currently only adapted option. section must still be adapted.

def convertCamelCase2Spaces(valueCC):
    s1 = re.sub('([a-z0-9])([A-Z])', r'\1 \2', valueCC)
    result=[]
    for token in s1.split(' '):
        u = token.upper()
        if u in [ "ACL", "CA", "CN", "DB", "DH", "FD", "LMDB", "NDMP", "PSK", "SD", "SSL", "TLS", "VSS" ]:
            token=u
        result.append(token)
    return " ".join(result)


def get_config_directive(text):
    '''
    This function generates from the signature
    the different required formats of a configuration directive.
    The signature (text) must be given as

    <dir|sd|fd|console>/<resourcetype_lower_case>/<DirectiveInCamelCase>

    For example:
    dir/job/TlsAlwaysIncrementalMaxFullAcl
    '''
    logger = logging.getLogger(__name__)
    displaynametemplate = u'{Directive} ({Dmn}->{Resource})'
    indextemplate  = u'Configuration Directive; ' + displaynametemplate

    # Latex: directiveDirJobCancel%20Lower%20Level%20Duplicates
    # The follow targettemplate will create identical anchors as Latex,
    # but as the base URL is likly it be different, it does not help (and still looks ugly).
    # targettemplate = u'directive{dmn}{resource}{directive}'
    targettemplate = u'config-{Dmn}_{Resource}_{CamelCaseDirective}'

    input_daemon = None
    input_resource = None
    try:
        input_daemon, input_resource, input_directive = text.split('/', 2)
    except ValueError:
        # fall back
        input_directive = text

    result = {
        'signature': text,
    }

    if input_daemon:
        daemon = input_daemon.lower()
        if daemon == 'director' or daemon == 'dir':
            result['Daemon'] = 'Director'
            result['dmn'] = 'dir'
            result['Dmn'] = 'Dir'
        elif daemon == 'storage daemon' or daemon == 'storage' or daemon == 'sd':
            result['Daemon'] = 'Storage Daemon'
            result['dmn'] = 'sd'
            result['Dmn'] = 'Sd'
        elif daemon == 'file daemon' or daemon == 'file' or daemon == 'fd':
            result['Daemon'] = 'File Daemon'
            result['dmn'] = 'fd'
            result['Dmn'] = 'Fd'
        elif daemon == 'bconsole' or daemon == 'console':
            result['Daemon'] = 'Console'
            result['dmn'] = 'console'
            result['Dmn'] = 'Console'
        else:
            result['Daemon'] = 'UNKNOWN'
            result['dmn'] = 'UNKNOWN'
            result['Dmn'] = 'UNKNOWN'

    else:
        result['Daemon'] = None
        result['Dmn'] = None
        result['dmn'] = None

    if input_resource:
        result['resource'] = input_resource.replace(' ', '').lower()
        result['Resource'] = input_resource.replace(' ', '').capitalize()
    else:
        result['resource'] = None
        result['Resource'] = None


    # input_directive should be without spaces.
    # However, we make sure, by removing all spaces.
    result['CamelCaseDirective'] = input_directive.replace(' ', '')
    result['Directive'] = convertCamelCase2Spaces(result['CamelCaseDirective'])

    result['displayname'] = displaynametemplate.format(**result)
    result['indexentry'] = indextemplate.format(**result)
    result['target'] = targettemplate.format(**result)

    logger.debug('[bareos] ' + pformat(result))

    return result



class ConfigOption(ObjectDescription):
    parse_node = None

    has_arguments = True

    doc_field_types = [
        Field('required', label='Required', has_arg=False,
              names=('required', )),
        Field('type', label='Type', has_arg=False,
              names=('type',)),
        Field('default', label='Default value', has_arg=False,
              names=('default', )),
        Field('version', label='Since Version', has_arg=False,
              names=('version', )),
        Field('deprecated', label='Deprecated', has_arg=False,
              names=('deprecated', )),
        Field('alias', label='Alias', has_arg=False,
              names=('alias', )),
    ]


    def handle_signature(self, sig, signode):
        directive = get_config_directive(sig)
        signode.clear()
        # only show the directive (not daemon and resource type)
        signode += addnodes.desc_name(sig, directive['Directive'])
        # normalize whitespace like XRefRole does
        name = ws_re.sub('', sig)
        return name


    def add_target_and_index(self, name, sig, signode):
        '''
        Usage:

        .. config:option:: dir/job/TlsEnable

           :required: True
           :type: Boolean
           :default: False
           :version: 16.2.4

           Multiline description ...

        The first argument specifies the directive and must be givenin following syntax:
        config::option:: <dir|sd|fd|console>/<resourcetype_lower_case>/<DirectiveInCamelCase>

        doc_field_types are only written when they should be displayed:
        :required: True
        :type: Boolean
        :default: False
        :version: 16.2.4
        :deprecated: True
        'alias: True

        To refer to this description, use
        :config:option:`dir/job/TlsEnable`.
        '''

        directive = get_config_directive(sig)

        targetname = directive['target']
        signode['ids'].append(targetname)
        self.state.document.note_explicit_target(signode)

        indextype = 'single'
        # Generic index entries
        self.indexnode['entries'].append((indextype, directive['indexentry'],
                                          targetname, targetname, None))

        self.env.domaindata['config']['objects'][self.objtype, sig] = \
            self.env.docname, targetname


class ConfigOptionXRefRole(XRefRole):
    """
    Cross-referencing role for configuration options (adds an index entry).
    """

    def result_nodes(self, document, env, node, is_ref):
        logger = logging.getLogger(__name__)
        logger.debug('[bareos] is_ref: {}, node[reftarget]: {}, {}'.format(str(is_ref), node['reftarget'], type(node['reftarget'])))

        if not is_ref:
            return [node], []

        varname = node['reftarget']

        directive = get_config_directive(varname)

        tgtid = 'index-%s' % env.new_serialno('index')

        indexnode = addnodes.index()
        indexnode['entries'] = [
            ('single', directive['indexentry'], tgtid, varname, None)
        ]
        targetnode = nodes.target('', '', ids=[tgtid])
        document.note_explicit_target(targetnode)

        return [indexnode, targetnode, node], []



    def process_link(self, env, refnode, has_explicit_title, title, target):
        if has_explicit_title:
            return title, target

        directive = get_config_directive(title)
        return directive['displayname'], target




class ConfigSection(ObjectDescription):
    indextemplate = 'configuration section; %s'
    parse_node = None

    def handle_signature(self, sig, signode):
        if self.parse_node:
            name = self.parse_node(self.env, sig, signode)
        else:
            signode.clear()
            signode += addnodes.desc_name(sig, sig)
            # normalize whitespace like XRefRole does
            name = ws_re.sub('', sig)
        return name

    def add_target_and_index(self, name, sig, signode):
        targetname = '%s-%s' % (self.objtype, name)
        signode['ids'].append(targetname)
        self.state.document.note_explicit_target(signode)
        if self.indextemplate:
            colon = self.indextemplate.find(':')
            if colon != -1:
                indextype = self.indextemplate[:colon].strip()
                indexentry = self.indextemplate[colon+1:].strip() % (name,)
            else:
                indextype = 'single'
                indexentry = self.indextemplate % (name,)
            self.indexnode['entries'].append((indextype, indexentry,
                                              targetname, targetname, None))
        self.env.domaindata['config']['objects'][self.objtype, name] = \
            self.env.docname, targetname


class ConfigSectionXRefRole(XRefRole):
    """
    Cross-referencing role for configuration sections (adds an index entry).
    """

    def result_nodes(self, document, env, node, is_ref):
        if not is_ref:
            return [node], []
        varname = node['reftarget']
        tgtid = 'index-%s' % env.new_serialno('index')
        indexnode = addnodes.index()
        indexnode['entries'] = [
            ('single', varname, tgtid, varname, None),
            ('single', 'configuration section; %s' % varname, tgtid, varname, None)
        ]
        targetnode = nodes.target('', '', ids=[tgtid])
        document.note_explicit_target(targetnode)
        return [indexnode, targetnode, node], []


class ConfigFileDomain(Domain):
    name = 'config'
    label = 'Config'

    object_types = {
            'option':  ObjType('config option', 'option'),
            'section':  ObjType('config section', 'section'),
            }
    directives = {
            'option': ConfigOption,
            'section': ConfigSection,
            }
    roles = {
            'option': ConfigOptionXRefRole(),
            'section': ConfigSectionXRefRole(),
            }

    initial_data = {
        'objects': {},      # (type, name) -> docname, labelid
    }

    def clear_doc(self, docname):
        toremove = []
        for key, (fn, _) in self.data['objects'].items():
            if fn == docname:
                toremove.append(key)
        for key in toremove:
            del self.data['objects'][key]

    def resolve_xref(self, env, fromdocname, builder,
                     typ, target, node, contnode):
        docname, labelid = self.data['objects'].get((typ, target), ('', ''))
        if not docname:
            return None
        else:
            return make_refnode(builder, fromdocname, docname,
                                labelid, contnode)

    def get_objects(self):
        for (type, name), info in self.data['objects'].items():
            yield (name, name, type, info[0], info[1],
                   self.object_types[type].attrs['searchprio'])


def autolink(urlpattern, textpattern = '{}'):
    def role(name, rawtext, text, lineno, inliner, options={}, content=[]):
        url = urlpattern.format(text)
        xtext = textpattern.format(text)
        node = nodes.reference(rawtext, xtext, refuri=url, **options)
        return [node], []
    return role


def bcommand():
    def role(name, rawtext, text, lineno, inliner, options={}, content=[]):

        env = inliner.document.settings.env
        
        try:
            command, parameter = text.split(' ', 1)
        except ValueError:
            command = text
            parameter = ''
        
        indexstring = 'Console; Command; {}'.format(command)
        targetid = 'bcommand-{}-{}'.format(command, env.new_serialno('bcommand'))
        
        # Generic index entries
        indexnode = addnodes.index()
        indexnode['entries'] = []

        indexnode['entries'].append([
            'single',
            indexstring,
            targetid, '', None
        ])

        targetnode = nodes.target('', '', ids=[targetid])

        text_node = nodes.strong(text='{}'.format(text))
        
        return [targetnode, text_node, indexnode], []
    return role


def os():
    
    #\newcommand{\os}[2]{\ifthenelse{\isempty{#2}}{%
        #\path|#1|\index[general]{Platform!#1}%
    #}{%
        #\path|#1 #2|\index[general]{Platform!#1!#2}%
    #}}    
    
    def role(name, rawtext, text, lineno, inliner, options={}, content=[]):

        env = inliner.document.settings.env

        # Generic index entries
        indexnode = addnodes.index()
        indexnode['entries'] = []
        
        try:
            platform, version = text.split(' ', 1)
            targetid = 'os-{}-{}-{}'.format(platform, version, env.new_serialno('os'))
            indexnode['entries'].append([
                'single',
                'Platform; {}; {}'.format(platform, version),
                targetid, '', None
            ])            
        except ValueError:
            platform = text
            version = ''
            targetid = 'os-{}-{}-{}'.format(platform, version, env.new_serialno('os'))            
            indexnode['entries'].append([
                'single',
                'Platform; {}'.format(platform),
                targetid, '', None
            ])

        targetnode = nodes.target('', '', ids=[targetid])

        text_node = nodes.strong(text='{}'.format(text))
        
        return [targetnode, text_node, indexnode], []
    return role


def sinceVersion():
    def role(name, rawtext, text, lineno, inliner, options={}, content=[]):
        #version = self.arguments[0]
        #summary = self.arguments[1]
        version, summary = text.split(':', 1)
        summary = summary.strip()
        
        indexstring = 'bareos-{}; {}'.format(version, summary)
        idstring = 'bareos-{}-{}'.format(version, summary)
        _id = nodes.make_id(idstring)
        
        # Generic index entries
        indexnode = addnodes.index()
        indexnode['entries'] = []

        indexnode['entries'].append([
            'pair',
            indexstring,
            _id, '', None
        ])

        targetnode = nodes.target('', '', ids=[_id])

        #text_node = nodes.Text(text='Version >= {}'.format(version))
        #text_node = nodes.strong(text='Version >= {}'.format(version))
        text_node = nodes.emphasis(text='Version >= {}'.format(version))
        # target does not work with generated.
        #text_node = nodes.generated(text='Version >= {}'.format(version))
        
        return [targetnode, text_node, indexnode], []
    return role


def setup(app):
    app.add_domain(ConfigFileDomain)
    app.add_role('bcommand', bcommand())
    app.add_role('os', os())    
    app.add_role('sinceversion', sinceVersion())
    app.add_role('ticket', autolink('https://bugs.bareos.org/view.php?id={}', 'Ticket #{}'))

    
    # identifies the version of our extension
    return {'version': '0.4'}
