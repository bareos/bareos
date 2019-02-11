# -*- coding: utf-8 -*-

from docutils import nodes
from docutils.parsers.rst import Directive
from docutils.parsers.rst import directives
from sphinx import addnodes
#from sphinx.util.docutils import SphinxDirective

class Limitation(Directive):

    # Define the parameters the directive expects
    required_arguments = 1
    optional_arguments = 0

    # A boolean, indicating if the final argument may contain whitespace
    final_argument_whitespace = True
    #option_spec = {'title', }
    has_content = True

    def run(self):

        env = self.state.document.settings.env

        # Raise an error if the directive does not have contents.
        self.assert_has_content()

        title = 'Limitation: ' + self.arguments[0]
        text  = '\n'.join(self.content)

        paragraph = nodes.paragraph()
        self.state.nested_parse(self.content, self.content_offset, paragraph)


        tgtid = 'index-%s' % env.new_serialno('index')
        indexnode = addnodes.index()
        indexnode['entries'] = [
                ('single', 'Limitation; ' + self.arguments[0] , tgtid , '', None)
                ]


        result = nodes.section(ids=['limitation'])
        result += nodes.title(text=title)
        #result += nodes.paragraph(text=text)
        result += paragraph
        result += indexnode

        return [result]

def setup(app):
    directives.register_directive("limitation", Limitation)
