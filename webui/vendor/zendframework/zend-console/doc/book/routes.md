# Console routes and routing

A powerful feature the zend-console component exposes is *routing*. Routing
reads the command line arguments and matches them to criteria; if the criteria
matches, it then returns a list of matched parameters and flags.

> ## Handling routing results
>
> zend-console exposes routing via the `Zend\Console\RouteMatcher\DefaultRouteMatcher`
> class, allowing you to create standalone routable console applications.
>
> zend-mvc provides structure around routing results to controllers, which
> we [detail in the MVC Routing chapter](mvc/routing.md).
>
> Another option is [zf-console](https://github.com/zfcampus/zf-console), which
> provides a convenience wrapper around supplying routes and dispatching route
> handlers.

## RouteMatcherInterface

zend-console defines an interface for routing,
`Zend\Console\RouteMatcher\RouteMatcherInterface`, which defines a single
method, `match()`:

```php
namespace Zend\Console\RouteMatcher;

interface RouteMatcherInterface
{
    /**
     * Match parameters against route passed to constructor
     *
     * @param array $params
     * @return array|null
     */
    public function match($params);
}
```

Applications are expected to retrieve arguments from the console and pass them to
the routing implementation as an array; the routing implementation will then
return either a `null` value (meaning failure to match), or an associative array
(the values matched).

## The default route matcher

zend-console's default routing implementation is `Zend\Console\RouteMatcher\DefaultRouteMatcher`.
Its constructor expects:

```php
    /**
     * @param string $route
     * @param array $constraints
     * @param array $defaults
     * @param array $aliases
     * @param array $filters
     * @param ValidatorInterface[] $validators
     * @throws Exception\InvalidArgumentException
     */
    public function __construct(
        $route,
        array $constraints = [],
        array $defaults = [],
        array $aliases = [],
        array $filters = null,
        array $validators = null
    )
```

The arguments are as follows:

- `$route` is a string pattern describing the argument list it will match.
- `$constraints` is an associative array, with the keys being named arguments,
  and the values being regular expressions to match against.
- `$defaults` are an an associative array of named arguments, with the default
  value to use if no value was provided; these should only be provided for
  optional arguments (more on that below).
- `$aliases` is an associative array where the key values are the alias names,
  and the string values are the named argument the alias points to; if an alias
  is used in the invocation, it will be returned using the named argument
  instead. As an example, if you provide the alias `'FOO' => 'foo'`, and have
  defined `--foo=` in the `$route`, you can invoke it with `--FOO=something`,
  and the router will map it to the `foo` value.
- `$filters` is an associative array of named arguments pointing to a
  `Zend\Filter\FilterInterface` instance (or `FilterChain` instance);
  filters are used to normalize the value associated with the argument.
- `$validators` is an associative array of named arguments pointing to a
  `Zend\Validator\ValidatorInterface` instance (or `ValidatorChain` instance);
  validators are used to validate values, and provide more options than simply
  regular expressions (as used with the `$constraints`).

> ### Single routes only
>
> `DefaultRouteMatcher` instances define *a single console route to match*. Most
> times, you will want to define multiple routes. The zend-mvc integration and
> zf-console both provide methods for aggregating routes.

## Routing strings

Routing strings consist of one or more of the following:

- [Literal parameters](#literal-parameters) (e.g. `create object (external|internal)`)
- [Literal flags](#literal-flags) (e.g. `--verbose --direct [-d] [-a]`)
- [Positional value parameters](#positional-value-parameters) (e.g. `create <modelName> [<destination>]`)
- [Value flags](#value-flag-parameters) (e.g. `--name=NAME [--method=METHOD]`)
- [Named literal alternative groups](#grouping-literal-alternatives) (e.g., `(all|some|none):filter`)

### Literal parameters

*Literal parameters* are expected to appear on the command line exactly the way
they are provided in the route. For example:

```
show users
```

This route will **only** match for the following command line

```bash
$ zf show users
```

It expects the **mandatory literal parameters** `show users`. It will not match
if there are any more parameters, or if either one of the two words is missing.
The order of words is also enforced.

You can also provide **optional literal parameters**. As an example:

```
show [all] users
```

The above route will match each of the following:

```bash
$ zf show users
$ zf show all users
```

You can also provide **parameter alternatives**:

```php
show [all|deleted|locked|admin] users
```

The above route will match both with and without the second parameter; if
provided, however, it must be one of the words listed. This enables matching any
of the following:

```php
$ zf show users
$ zf show locked users
$ zf show admin users
# etc.
```

> #### Whitespace is ignored
>
> Whitespaces in route definitions are ignored. If you separate your parameters
> with more spaces, or separate alternatives and pipe characters with spaces,
> the parser will ignore the whitespace.
> The above route definition is equivalent to:
>
> ```
> show [  all | deleted | locked | admin  ]   users
> ```
>
> As such, you can use whitespace for readability.

### Literal flags

Console tools commonly use flags. zend-console allows you to define any number
of optional and/or mandatory flags.

Flag order is ignored; they can be defined in any order, and the user can
provide them in any order.

The following is a route with **optional long flags**:

```
check users [--verbose] [--fast] [--thorough]
```

The above route will match commands like:

```bash
$ zf check users
$ zf check users --fast
$ zf check users --verbose --thorough
$ zf check users --thorough --fast
# etc
```

You can also define one or more **mandatory long flags**, and group them as an
alternative:

```php
check users (--suspicious|--expired) [--verbose] [--fast] [--thorough]
```

The above will only match if we provide either the `--suspicious` or `--expired`
flag:

```bash
$ zf check users --expired
$ zf check users --expired --fast
$ zf check users --verbose --thorough --suspicious
```

**Short flags** are also available, and may be grouped with long flags for
convenience:

```
check users [--verbose|-v] [--fast|-f] [--thorough|-t]
```

Now we can use short versions of our flags:

```bash
$ zf check users -f
$ zf check users -v --thorough
$ zf check users -t -f -v
# etc.
```

### Positional value parameters

Value parameters capture any text-based input, and come in two forms: positional
and flags (which we've already discussed).

**Positional value parameters** are expected to appear in an *exact* position on
the command line, and are denoted using angle brackets (`<>`).

Consider the following:

```
delete user <userEmail>
```

This route will match the following commands:

```bash
$ zf delete user john@acme.org
$ zf delete user betty@acme.org
```

When matched, the router will return the value under the key we provided in the
route definition. If using the `DefaultRouteMatcher` standalone, this would be:

```php
$matches = $route->match($argv);
$userEmail = $matches['userEmail'];
```

Under zend-mvc, you will pull the parameter from the request:

```php
$userEmail = $this->getRequest()->getParam('userEmail');
```

You can also define **optional positional value parameters** by surrounding the
parameter with square brackets:

```
delete user [<userEmail>]
```

In this case, the `userEmail` parameter will not be required for the route to
match. If it is not provided, the `userEmail` parameter will not be present in
the matched parameters.

You can define any number of positional value parameters:

```
create user <firstName> <lastName> <email> <position>
```

This allows us to capture commands such as the following:

```bash
$ zf create user Johnny Bravo john@acme.org Entertainer
```

> #### Escaping
>
> Command line arguments on all systems must be properly escaped; otherwise they
> will not be passed to our application correctly. For example, to create a user
> with two names and a complex position description, we would issue the command
> like this:
>
> ```bash
> $ zf create user "Johnan Tom" Bravo john@acme.org "Head of the Entertainment Department"
> ```

### Value flag parameters

Positional value parameters are only matched if they appear in the exact order described in the
route. If we do not want to enforce the order of parameters, we can define **value flags**.

**Value flags** can be defined and matched in *any* order, and can receive any
text-based value.

```
find user [--id=] [--firstName=] [--lastName=] [--email=] [--position=]
```

The above route will match for any of the following routes:

```bash
$ zf find user
$ zf find user --id 29110
$ zf find user --id=29110
$ zf find user --firstName=Johny --lastName=Bravo
$ zf find user --lastName Bravo --firstName Johny
$ zf find user --position=Executive --firstName=Bob
$ zf find user --position "Head of the Entertainment Department"
# etc.
```

As noted, the order of flags is irrelevant for the parser.

> ##### Providing values
>
> The parser understands values that are provided after either an equals symbol
> (`=`) or a single space, but only if the value itself does not contain
> whitespace.  Values containing any whitespace  must be properly quoted and
> appear following a space *only*; you cannot use the `=` sign to assign such
> values.

In the previous example, all value flags are optional. You may also define
**mandatory value flags** by omitting the square brackets:

```
rename user --id= [--firstName=] [--lastName=]
```

In the above example, the `--id` parameter **is required** for the route to
match. The following commands will work with this route:

```bash
$ zf rename user --id 123
$ zf rename user --id 123 --firstName Jonathan
$ zf rename user --id=123 --lastName=Bravo
# etc.
```

### Grouping literal alternatives

In the flags section, we demonstrated grouping alternative flags:

```
check users (--suspicious|--expired) [--verbose] [--fast] [--thorough]
```

This can also be done with literals:

```
show (all|deleted|locked|admin) <group>
```

However, this makes checking for which alternative was used quite difficult:

```php
switch (true) {
    case (isset($params['all'])):
        // all members
        break;
    case (isset($params['deleted'])):
        // deleted members
        break;
    /* etc. */
}
```

To simplify this, you can *assign a name to the grouped alternatives*. Do this
with the verbiage `:groupname` following the group:

```
show (all|deleted|locked|admin):filter <group>
```

The above names the group "filter". When a group is provided a name, you can
then retrieve the group name parameter, which will be set to the alternative
used:

```php
switch ($params['filter']) {
    case 'all':
        // all members
        break;
    case 'deleted':
        // deleted members
        break;
    /* etc. */
}
```

## Console routes cheat-sheet

Param type                        | Example route definition    | Explanation
:---                              | :---                        | :---
**Literal params**                |                             |
Literal                           | `foo bar`                   | "foo" followed by "bar"
Literal alternative               | `foo (bar|baz)`             | "foo" followed by "bar" or "baz"
Literal, optional                 | `foo [bar]`                 | "foo", optional "bar"
Literal, optional alternative     | `foo [bar|baz]`             | "foo", optional "bar" or "baz"
**Flags**                         |                             |
Flag long                         | `foo --bar`                 | "foo" as first parameter, "--bar" flag before or after
Flag long, optional               | `foo [--bar]`               | "foo" as first parameter, optional "--bar" flag before or after
Flag long, optional, alternative  | `foo [--bar|--baz]`         | "foo" as first parameter, optional "--bar" or "--baz", before or after
Flag short                        | `foo -b`                    | "foo" as first parameter, "-b" flag before or after
Flag short, optional              | `foo [-b]`                  | "foo" as first parameter, optional "-b" flag before or after
Flag short, optional, alternative | `foo [-b|-z]`               | "foo" as first parameter, optional "-b" or "-z", before or after
Flag long/short alternative       | `foo [--bar|-b]`            | "foo" as first parameter, optional "--bar" or "-b" before or after
**Value parameters**              |                             |
Value positional param            | `foo <bar>`                 | "foo" followed by any text (stored as "bar" param)
Value positional param, optional  | `foo [<bar>]`               | "foo", optionally followed by any text (stored as "bar" param)
Value Flag                        | `foo --bar=`                | "foo" as first parameter, "--bar" with a value, before or after
Value Flag, optional              | `foo [--bar=]`              | "foo" as first parameter, optionally "--bar" with a value, before or after
**Parameter groups**              |                             |
Literal params group              | `foo (bar|baz):myParam`     | "foo" followed by "bar" or "baz" (stored as "myParam" param)
Literal optional params group     | `foo [bar|baz]:myParam`     | "foo" followed by optional "bar" or "baz" (stored as "myParam" param)
Long flags group                  | `foo (--bar|--baz):myParam` | "foo", "bar" or "baz" flag before or after (stored as "myParam" param)
Long optional flags group         | `foo [--bar|--baz]:myParam` | "foo", optional "bar" or "baz" flag before or after (as "myParam" param)
Short flags group                 | `foo (-b|-z):myParam`       | "foo", "-b" or "-z" flag before or after (stored as "myParam" param)
Short optional flags group        | `foo [-b|-z]:myParam`       | "foo", optional "-b" or "-z" flag before or after (stored as "myParam" param)
