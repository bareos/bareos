# Bareos Source Sanity Checker

This repository contains a pluggable python-based source sanity checker targeted to check all of the Bareos sources.

The program selects files in your source directory to look at based on their git status (i.e. committed, changed, untracked). For each file all the plugins will be run and will check if the file is ok and - if possible and enabled - show what needs to be changed or apply the changes automatically.

## Installation
The program was developed and tested with python 3.6, so this is the minimum required version (we cannot describe this in the Pipfile).
First of all, you need the virtualenv for the project. To install all required dependencies, just `pipenv sync` inside the `pip-tools` directory.
After that you can run included software with symlinks pointing to the supplied `pipenv-wrapper.sh`. If you add a symlink `bareos-check-sources` from one of your binary dirs (e.g. ~/.local/bin) to that script you'll have it available in your path.
When updating, you may need to rerun `pipenv sync` up update dependencies.

### Upgrading `bareos-check-sources` or your python installation
When you upgrade the software or your python install, you should clear out the virtualenv using `pipenv --rm` and the recreate it using `pipenv sync`.

## Usage
See `bareos-check-sources --help` for current online-documentation.
By default bareos-check-sources runs all installed plugins on all changed files in the git repository it is invoked from. To also check untracked files, you can use the `--untracked` or `-u` option.
To select more files, you can run it with `--since=<commit-ref>` to check everything changed since that commit, `--since-merge` to check all files changed since the last merge commit or `--all` to just check all files (this may take a very long time)

By default all plugins will check and report if changes are required. For modifier plugins, you can also add `--modify` or `--diff` as a parameter. These will write-back the changes and/or show a diff of what needs to be changed.

While the default is to load all plugins, you can select specific plugins by using the `--plugin` option.

## Currently implemented checks/modifiers
* clang-format
* cmake-format
* python black
* trailing spaces
* trailing newlines
* dos line-endings
* missing copyright notices
* copyright notices with wrong end-year
* merge conflicts
* executables have a shebang
* python executables have a sane shebang
* check C/C++ header guards
* check/fix comments wasting space

## Extending
All plugins reside in the `plugins` package and will be autoloaded on startup. To add a new plugin, you can simply drop a new module-file in the `plugins` directory. Every plugin is registered as a checker or a modifier for one or more glob patterns. The registered function will be called with the filename and the file contents as parameters. While a checker returns `True`/`False`, a modifier needs to return the file's content as it should look like.

## TODO
* add more plugins that help improve the code and find problems
	* ShellCheck
	* phplint
	* cmake-lint (in addition to cmake-format)
	* check for C constructs we consider bad practice (e.g. "NULL")
	* use "long switches" when calling binaries from shell scripts
