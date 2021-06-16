# Zend\\Console\\Getopt

The `Zend\Console\Getopt` class helps command-line applications to parse their
options and arguments.

Users may specify command-line arguments when they execute your application. These arguments have
meaning to the application, to change the behavior in some way, or choose resources, or specify
parameters. Many options have developed customary meaning, for example `--verbose` enables extra
output from many applications. Other options may have a meaning that is different for each
application. For example, `-c` enables different features in `grep`, `ls`, and `tar`.

Below are a few definitions of terms. Common usage of the terms varies, but this documentation will
use the definitions below.

- "argument": a string that occurs on the command-line following the name of the
  command. Arguments may be options or else may appear without an option, to
  name resources on which the command operates.
- "option": an argument that signifies that the command should change its
  default behavior in some way.
- "flag": the first part of an option, identifies the purpose of the option. A
  flag is preceded conventionally by one or two dashes (`-` or `--`). A single
  dash precedes a single-character flag or a cluster of single-character flags.
  A double-dash precedes a multi-character flag. Long flags cannot be clustered.
- "parameter": the secondary part of an option; a data value that may accompany
  a flag, if it is applicable to the given option. For example, many commands
  accept a `--verbose` option, but typically this option has no parameter.
  However, an option like `--user` almost always requires a following parameter.

  A parameter may be given as a separate argument following a flag argument, or
  as part of the same argument string, separated from the flag by an equals symbol
  (`=`). The latter form is supported only by long flags. For example, `-u
  username`, `--user username`, and `--user=username` are forms supported by
  `Zend\Console\Getopt`.
- "cluster": multiple single-character flags combined in a single string
  argument and preceded by a single dash. For example, "`ls -1str`" uses a
  cluster of four short flags. This command is equivalent to "`ls -1 -s -t -r`".
  Only single-character flags can be clustered. You cannot make a cluster of
  long flags.

For example, in `mysql --user=root mydatabase`, `mysql` is a **command**,
`--user=root` is an **option** (`--user` is a **flag** and `root` is a
**parameter** to the option), and `mydatabase` is an argument (but not an option
by our definition).

`Zend\Console\Getopt` provides an interface to declare which flags are valid for
your application, output an error and usage message if they invalid flags are
specified, and report to your application which flags the user specified.

> ## Getopt is not an Application Framework
>
> `Zend\Console\Getopt` does **not** interpret the meaning of flags and
> parameters, nor does this class implement application workflow or invoke
> application code. You must implement those actions in your own application
> code. You can use the `Zend\Console\Getopt` class to parse the command-line
> and provide object-oriented methods for querying which options were given by a
> user, but code to use this information to invoke parts of your application
> should be in another PHP class.

The following sections describe usage of `Zend\Console\Getopt`.
