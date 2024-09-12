# Use plain text for Resource Item Descriptions

* Status: accepted
* Deciders: [Jörg Steffens], [Stephan Dühr], [Andreas Rogge], [Bruno Friedmann], [Sebastian Sura]
* Date: 2024-04-09

Technical Story: [PR #1761](https://github.com/bareos/bareos/pull/1761) raised the question how to format those

## Context and Problem Statement

As `*all*` for the ACL was formatted as italic, but without the asterisks, we
tried to solve that by adding RST formatted text into the C source files.


## Considered Options

* Just use RST formatted text
* Only use plain text that won't be formatted by Sphinx
* Implement parser to escape RST things in the text, so Sphinx build produces
  nice output
* Just use the verbatim text in Sphinx

## Decision Outcome

Chosen option: "Only use plain text", because including RST formatting in that
place stops us from using that texts in other places in the future.

Those descriptions in the C source should be kept short.
If a detailed description is required, it should be written to the RST file
that is automatically generated in the docs.

In addition to that the converter that creates the RST files for that will:
- highlight text by surrouding it with double-quotes (`"`) (don't forget to
  escape the C-string correctly)
- convert a word that starts and ends with an asterisk (`*`) as strong formatted
  (i.e. `*all*` will be formatted bold in the output, instead of producing an
  italic "all" which would be the default behaviour of RST)
- fail if the string contains backticks (`` ` ``) or (`**`)

### Positive Consequences

* provides a unified way to describe resource items
* allows re-use of the texts in other contexts (i.e. online-help)
* the `--export-schema` only contains plain-text descriptions

### Negative Consequences

* harder to update the descriptions
* less options to format the descriptions
* scattering of information between C code and documentation

## Pros and Cons of the Options <!-- optional -->

### Just use RST formatted text

* Bad, because it won't be reusable
* Bad, because it is not really easy to read when mixed with C code
* Good, because it is easy to create nicely formatted descriptions in the docs

### Implement parser to escape RST things

* Good, because it would simplify writing of resource item descriptions
* Bad, because it is hard to implement and maintain

### Just use the verbatim text in Sphinx

* Good, because it would simplify writing of resource item descriptions
* Bad, because it is either monospaced text or too much work to implement in
  Sphinx

[Andreas Rogge]: https://github.com/arogge
[Bruno Friedmann]: https://github.com/bruno-at-bareos
[Jörg Steffens]: https://github.com/joergsteffens
[Sebastian Sura]: https://github.com/sebsura
[Stephan Dühr]: https://github.com/sduehr
