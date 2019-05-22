# -*- coding: utf-8 -*-

from docutils import nodes
from docutils.parsers.rst import Directive
from docutils.parsers.rst import directives
from sphinx import addnodes

class limitation(nodes.Admonition, nodes.Element):
    pass

def visit_limitation_node(self, node):
    self.visit_admonition(node)

def depart_limitation_node(self, node):
    self.depart_admonition(node)

class LimitationDirective(Directive):

    # Define the parameters the directive expects
    required_arguments = 1
    optional_arguments = 0

    # A boolean, indicating if the final argument may contain whitespace
    final_argument_whitespace = True
    has_content = True


    def run(self):

        env = self.state.document.settings.env

        component = None
        summary = ''
        try:
            component, summary = self.arguments[0].split(':', 1)
        except ValueError:
            summary = self.arguments[0]
        title = 'Limitation: ' + self.arguments[0]
        #indextext = 'Limitation; ' + self.arguments[0]

        # Generic index entries
        indexnode = addnodes.index()
        indexnode['entries'] = []

        if component:
            title = 'Limitation - {}: {}'.format(component, summary)
            _id = nodes.make_id(title)
            
            # indices:
            #   Limitation; Component; Summary
            #   Component; Summary; Limitation
            #   Component; Limitation; Summary

            indexnode['entries'].append([
                'single',
                'Limitation; {}; {}'.format(component, summary),
                _id, '', None
            ])
            indexnode['entries'].append([
                'single',
                '{}; {}; Limitation'.format(component, summary),
                _id, '', None
            ])
            indexnode['entries'].append([
                'single',
                '{}; Limitation; {}'.format(component, summary),
                _id, '', None
            ])
        else:
            # if limitation is specified without an component
            title = 'Limitation: {}'.format(summary)
            _id = nodes.make_id(title)
            
            # indices:
            #   Limitation; Summary
            #   Summary; Limitation

            indexnode['entries'].append([
                'pair',
                'Limitation; {}'.format(summary),
                _id, '', None
            ])

        targetnode = nodes.target('', '', ids=[_id])

        limitation_node = limitation()
        limitation_node += nodes.title(text=title)
        self.state.nested_parse(self.content, self.content_offset, limitation_node)

        return [targetnode, limitation_node, indexnode]



def setup(app):
    app.add_node(limitation,
                 html=(visit_limitation_node, depart_limitation_node),
                 latex=(visit_limitation_node, depart_limitation_node),
                 text=(visit_limitation_node, depart_limitation_node)
                 )
    app.add_directive('limitation', LimitationDirective)

    # identifies the version of our extension
    return {'version': '0.2'}
