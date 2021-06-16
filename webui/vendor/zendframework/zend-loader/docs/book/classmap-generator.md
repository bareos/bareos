# The Class Map Generator utility: bin/classmap\_generator.php

The script `bin/classmap_generator.php` can be used to generate class map files for use with the
[ClassMapAutoloader](class-map-autoloader.md).

Internally, it consumes both the [zend-console getopt functionality](https://docs.zendframework.com/zend-console/getopt/intro/)
(for parsing command-line options) and the [zend-file ClassFileLocator](https://docs.zendframework.com/zend-file/class-file-locator/)
for recursively finding all PHP class files in a given tree.

## Quick Start

You may run the script over any directory containing source code. By default, it
will look in the current directory, and will write the script to
`autoloader_classmap.php` in the directory you specify.

```bash
$ php classmap_generator.php Some/Directory/
```

## Configuration Options

Option             | Description
------------------ | -----------
`--help | -h`      | Returns the usage message. If any other options are provided, they will be ignored.
`--library | -l`   | Expects a single argument, a string specifying the library directory to parse. If this option is not specified, it will assume the current working directory.
`--output | -o`    | Where to write the autoload class map file. If not provided, assumes `autoload_classmap.php` in the library directory.
`--append | -a`    | Append to autoload file if it exists.
`--overwrite | -w` | If an autoload class map file already exists with the name as specified via the `--output` option, you can overwrite it by specifying this flag. Otherwise, the script will not write the class map and return a warning.
