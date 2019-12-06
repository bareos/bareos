
# --------------------------
# General Formatting Options
# --------------------------
# How wide to allow formatted cmake files
line_width = 80

# How many spaces to tab for indent
tab_size = 2

# If an argument group contains more than this many sub-groups (parg or kwarg
# groups), then force it to a vertical layout.
max_subgroups_hwrap = 2

# If a positional argument group contains more than this many arguments, then
# force it to a vertical layout.
max_pargs_hwrap = 8

# If true, separate flow control names from their parentheses with a space
separate_ctrl_name_with_space = False

# If true, separate function names from parentheses with a space
separate_fn_name_with_space = False

# If a statement is wrapped to more than one line, than dangle the closing
# parenthesis on it's own line.
dangle_parens = True

# If the trailing parenthesis must be 'dangled' on it's on line, then align it
# to this reference: `prefix`: the start of the statement,  `prefix-indent`: the
# start of the statement, plus one indentation  level, `child`: align to the
# column of the arguments
dangle_align = 'prefix'

min_prefix_chars = 4

# If the statement spelling length (including space and parenthesis is larger
# than the tab width by more than this among, then force reject un-nested
# layouts.
max_prefix_chars = 10

# If a candidate layout is wrapped horizontally but it exceeds this many lines,
# then reject the layout.
max_lines_hwrap = 2

# What style line endings to use in the output.
line_ending = 'unix'

# Format command names consistently as 'lower' or 'upper' case
command_case = 'canonical'

# Format keywords consistently as 'lower' or 'upper' case
keyword_case = 'unchanged'

# Specify structure for custom cmake functions
additional_commands = {
  "bareos_add_test": {
    "pargs": 1,
    "flags": [
      "",
      ""
    ],
    "kwargs": {
      "ADDITIONAL_SOURCES": "*",
      "LINK_LIBRARIES": "*",
      "COMPILE_DEFINITIONS": "*"
    }
  }
}

# A list of command names which should always be wrapped
always_wrap = []

# If true, the argument lists which are known to be sortable will be sorted
# lexicographically
enable_sort = True

# If true, the parsers may infer whether or not an argument list is sortable
# (without annotation).
autosort = False

# If a comment line starts with at least this many consecutive hash characters,
# then don't lstrip() them off. This allows for lazy hash rulers where the first
# hash char is not separated by space
hashruler_min_length = 10

# A dictionary containing any per-command configuration overrides. Currently
# only `command_case` is supported.
per_command = {}

# A dictionary mapping layout nodes to a list of wrap decisions. See the
# documentation for more information.
layout_passes = {}


# --------------------------
# Comment Formatting Options
# --------------------------
# What character to use for bulleted lists
bullet_char = '*'

# What character to use as punctuation after numerals in an enumerated list
enum_char = '.'

# enable comment markup parsing and reflow
enable_markup = True

# If comment markup is enabled, don't reflow the first comment block in each
# listfile. Use this to preserve formatting of your copyright/license
# statements.
first_comment_is_literal = True

# If comment markup is enabled, don't reflow any comment block which matches
# this (regex) pattern. Default is `None` (disabled).
literal_comment_pattern = None

# Regular expression to match preformat fences in comments
# default=r'^\s*([`~]{3}[`~]*)(.*)$'
fence_pattern = '^\\s*([`~]{3}[`~]*)(.*)$'

# Regular expression to match rulers in comments
# default=r'^\s*[^\w\s]{3}.*[^\w\s]{3}$'
ruler_pattern = '^\\s*[^\\w\\s]{3}.*[^\\w\\s]{3}$'

# If true, then insert a space between the first hash char and remaining hash
# chars in a hash ruler, and normalize it's length to fill the column
canonicalize_hashrulers = True


# ---------------------------------
# Miscellaneous Options
# ---------------------------------
# If true, emit the unicode byte-order mark (BOM) at the start of the file
emit_byteorder_mark = False

# Specify the encoding of the input file. Defaults to utf-8.
input_encoding = 'utf-8'

# Specify the encoding of the output file. Defaults to utf-8. Note that cmake
# only claims to support utf-8 so be careful when using anything else
output_encoding = 'utf-8'
