# -*- coding: utf-8 -*-

from docutils import nodes
from docutils.parsers.rst import Directive
from docutils.parsers.rst import directives
from sphinx import addnodes

class Limitation(Directive):

    # Define the parameters the directive expects
    required_arguments = 1
    optional_arguments = 0

    # A boolean, indicating if the final argument may contain whitespace
    final_argument_whitespace = True
    has_content = True

    def run(self):

        env = self.state.document.settings.env

        # Content text is optional, therefore don't raise an exception.
        # Raise an error if the directive does not have contents.
        #self.assert_has_content()

        # TODO: parse title
        title = 'Limitation: ' + self.arguments[0]
        indextext = 'Limitation; ' + self.arguments[0]

        _id = nodes.make_id(title)

        paragraph = nodes.paragraph()
        self.state.nested_parse(self.content, self.content_offset, paragraph)

        # Generic index entries
        indexnode = addnodes.index()
        indexnode['entries'] = [
            ('single', indextext, _id, '', None)
        ]

        result = nodes.section(ids=[_id])
        result += nodes.title(text=title)
        result += paragraph
        result += indexnode

        return [result]

def setup(app):
    directives.register_directive("limitation", Limitation)
