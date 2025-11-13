# Plugin virtual filenames

* Status: accepted
* Deciders: [Bruno Friedmann], [Andreas Rogge], [Philipp Storz], [Sebastian Sura]
* Date: 2025-11-13

## Context and Problem Statement

Bareos plugins create virtual files to represent content in the restore tree.
Currently, there is no standard how the filenames should be generated, leading
to a wild mix variations that mainly represent the opinion of the plugin
developer at time of development.

## Decision Drivers

* new plugins want to do it "right", but nobody knows what that means
* no immediate way to tell if some data requires a plugin for restore

## Considered Options

* Just leave it as is
* Prefix with `@<PLUGIN-NAME>/`
* Prefix with `@<plugin-name>@/`
* Prefix with `<PLUGIN-NAME>:/`

## Decision Outcome

Chosen option: "Prefix with `@<PLUGIN-NAME>/`", because that is what the
majority of already shipped plugins use, and it has no major drawbacks compared
to the other options.

Plugins can also create non-virtual files. That is if the plugin backs up
actual files. Such a plugin *must not* mutate the data on restore. That means
when restoring on a file daemon with plugins disabled, you will get the
equivalent result that you would get from a restore using the plugin.
These plugins *may* use the actual path of the file being backed up instead of
adhering to the standard defined here.

### Positive Consequences

* plugin data is immediately recognizable in the restore tree
* the plugin needed to restore the data is apparent
* paths will not clash with other plugins or file backups

### Negative Consequences

* existing plugins will have to be updated backwards-compatible 
* providing and retaining a clean upgrade path could be challenging
* upgrading a plugin may break a user's workflow

## Pros and Cons of the Options

### Just leave it as is

* Good, because it won't break existing workflows
* Good, because we don't need to change existing plugins
* Good, because we don't need to identify acceptable exceptions
* Good, because no backwards compatibility issues
* Bad, because it is not standardized
* Bad, because plugin content is not easily identifiable in the restore tree
* Bad, because plugin content can "hide" in the restore tree

### Prefix with `@<PLUGIN-NAME>/`

Prefix paths with is a single at symbol (`@`) immediately followed the short
name of the plugin, in uppercase letters, with words separated by hyphens (`-`)
(a.k.a. "SCREAMING-KEBAB-CASE" or "COBOL-CASE") and ending in a single forward
slash (`/`) that will function as a directory separator.

For example, a python plugin `bareos-fd-plumbus.py` would create files in the
format `@PLUMBUS/actual-filename` or `@PLUMBUS/some-dir/the-filename`.

* Good, because it cannot clash with file backups
* Good, because it won't clash with other plugins
* Good, because jobs using plugins can be identified with a simple SQL query
* Bad, because it will require backwards-compatible changes to existing plugins
* Bad, because changes to existing paths could potentially break a user's workflow

### Prefix with `@<plugin-name>@/`

Prefix paths with is a single at symbol (`@`) immediately followed the short
name of the plugin, in lowercase letters, with words separated by hyphens (`-`)
(a.k.a. "kebab-case" or "spinal-case"), followed by another at symbol (`@`),
and ending in a single forward slash (`/`) that will function as a directory
separator.

For example, a python plugin `bareos-fd-plumbus.py` would create files in the
format `@PLUMBUS/actual-filename` or `@PLUMBUS/some-dir/the-filename`.

* Good, because it cannot clash with file backups
* Good, because it won't clash with other plugins
* Good, because jobs using plugins can be identified with a simple SQL query
* Bad, because it will require backwards-compatible changes to existing plugins
* Bad, because changes to existing paths could potentially break a user's workflow
* Bad, because the second at symbol does not add value
* Bad, because it requires changes to more plugins than `@<PLUGIN-NAME>/`

### Prefix with `<PLUGIN-NAME>:/`

Prefix paths with the name of the plugin, in uppercase letters, with words
separated by hyphens (`-`) (a.k.a. "SCREAMING-KEBAB-CASE" or "COBOL-CASE"),
followed by a single colon (`:`) and ending in a single forward slash (`/`)
that will function as a directory separator.

For example, python plugin `bareos-fd-plumbus.py` would create files in the
format `PLUMBUS:/actual-filename` or `PLUMBUS:/some-dir/the-filename`.

* Good, because it cannot clash with file backups
* Good, because it won't clash with other plugins
* Good, because jobs using plugins can be identified with a simple SQL query
* Bad, because it will require backwards-compatible changes to existing plugins
* Bad, because changes to existing paths could potentially break a user's workflow
* Bad, because not easily restorable on Windows when the plugin is missing
* Bad, because it requires changes to more plugins than `@<PLUGIN-NAME>/`

[Bruno Friedmann]: https://github.com/bruno-at-bareos
[Andreas Rogge]: https://github.com/arogge
[Sebastian Sura]: https://github.com/sebsura
[Philipp Storz]: https://github.com/pstorz
