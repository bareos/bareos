# Declaring Getopt Rules

The constructor for the `Zend\Console\Getopt` class takes from one to three
arguments. The first argument declares which options are supported by your
application. This class supports alternative syntax forms for declaring the
options. See the sections below for the format and usage of these syntax forms.

The constructor takes two more arguments, both of which are optional. The second
argument may contain the command-line arguments. This defaults to
`$_SERVER['argv']`.

The third argument of the constructor may contain any configuration options
required to customize the behavior of `Zend\Console\Getopt`. See
[Adding Configuration](configuration.md#adding-configuration) for a reference on
the options available.

## Declaring Options with the Short Syntax

`Zend\Console\Getopt` supports a compact syntax similar to that used by
[GNU Getopt](http://www.gnu.org/software/libc/manual/html_node/Getopt.html).
This syntax supports only single-character flags. In a single string, you type
each of the letters that correspond to flags supported by your application. A
letter followed by a colon character (`:`) indicates a flag that requires a
parameter.

### Using the Short Syntax

```php
$opts = new Zend\Console\Getopt('abp:');
```

The example above shows using `Zend\Console\Getopt` to declare that options may
be given as `-a`, `-b`, or `-p`. The latter flag requires a parameter.

The short syntax is limited to flags of a single character. Aliases, parameter
types, and help strings are not supported in the short syntax.

## Declaring Options with the Long Syntax

A different syntax with more features is also available. This syntax allows you
to specify aliases for flags, types of option parameters, and also help strings
to describe usage to the user. Instead of the single string used in the short
syntax to declare the options, the long syntax uses an associative array as the
first argument to the constructor.

The key of each element of the associative array is a string with a format that
names the flag, with any aliases, separated by the pipe symbol (`|`). Following
this series of flag aliases, if the option requires a parameter, is an equals
symbol (`=`) with a letter that stands for the **type** of the parameter:

- `=s` for a string parameter
- `=w` for a word parameter (a string containing no whitespace)
- `=i` for an integer parameter

If the parameter is optional, use a dash (`-`) instead of the equals symbol.

The value of each element in the associative array is a help string to describe
to a user how to use your program.

### Using the Long Syntax

```php
$opts = new Zend\Console\Getopt(
  array(
    'apple|a'    => 'apple option, with no parameter',
    'banana|b=i' => 'banana option, with required integer parameter',
    'pear|p-s'   => 'pear option, with optional string parameter'
  )
);
```

In the example declaration above, there are three options. `--apple` and `-a`
are aliases for each other, and the option takes no parameter. `--banana` and
`-b` are aliases for each other, and the option takes a mandatory integer
parameter. Finally, `--pear` and `-p` are aliases for each other, and the option
may take an optional string parameter.
