from docutils import nodes
from docutils.parsers.rst import Directive

class Limitation(nodes.Structural, nodes.Element):
    pass

class LimitationDirective(Directive):

    # Define the parameters the directive expects
    required_arguments = 0
    optional_arguments = 0

    # A boolean, indicating if the final argument may contain whitespace
    final_argument_whitespace = True
    option_spec = {'title', }
    has_content = True

    def run(self):
        # Raise an error if the directive does not have contents.
        sett = self.state.document.settings
        env = self.state.document.settings.env

        config = env.config

        self.assert_has_content()
        text = '\n'.join(self.content)

        # Get access to the options of directive
        options = self.options

        # Create the limitation content, to be populated by `nested_parse`.
        limitation_content = nodes.paragraph(rawsource=text)

        # Parse the directive contents.
        self.state.nested_parse(self.content, self.content_offset, limitation_content)

        # Create a title
        limitation_title = nodes.title("Limitations: " + options["title"])

        # Create the limitation node
        node = Limitation()
        node += limitation_title
        node += limitation_content

        # Return the result
        return [node]

#class Limitation(LimitationDirective):
 #   pass

def setup(app):
    app.add_node(Limitation)
    app.add_directive("limitation", LimitationDirective)
