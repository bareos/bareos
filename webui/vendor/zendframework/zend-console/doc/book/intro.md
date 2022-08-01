# Introduction to zend-console

zend-console provides both generic support for routable console applications, as
well as the basis for adding console support to zend-mvc-based applications.

When a zend-mvc application is run from a console window (a shell window or
Windows command prompt), it will recognize this fact and prepare zend-mvc
components to handle the request. Console support is enabled by default, but to
function properly it requires at least one [console route](routes.md) and one
[action controller](http://framework.zend.com/manual/current/zend.mvc.controllers.html)
to handle the request.

Alternately, you can build console-aware applications that are standalone;
[zf-console](https://github.com/zfcampus/zf-console) provides a standardized
workflow for doing so.

Features in zend-console include:

- [Console routing](routes.md) allows you to invoke handlers (including
  controller actions) depending on command line parameters provided by the user.
- [Console adapters](adapter.md) provide abstraction for interacting with
  consoles exposed by different operating systems.
- [Console prompts](prompts.md) allow user interaction by asking questions and
  retrieving input.

When used with [zend-mvc](https://github.com/zendframework/zend-mvc), the
following features are also available:

- [Module Manager integration](mvc/modules.md) allows zend-mvc applications and
  modules to display help and usage information, either on demand, or in
  situations where no route was matched.
- [Console-aware action controllers](mvc/controllers.md) will receive a console
  request containing all named parameters and flags. They are able to send
  output back to the console window.

## Quick Start with zend-mvc

> The following example details integration of zend-console with zend-mvc.
> However, all information pertaining to routing can also be used *without*
> zend-mvc, but requires that you react to the routing results yourself. The
> aforementioned zf-console provides some basic wiring around that, and can and
> should be consulted if you do not choose to use zend-console with zend-mvc.

A console route defines required and optional command line parameters. When a
route matches, it returns an array of matched parameters &mdash; which may
include default parameters, or even parameters not exposed via the command line
(e.g., metadata related to the route).

Let's assume that we'd like our application to handle the following command line:

```bash
$ zf user resetpassword user@mail.com
```

When a user runs our application (`zf`) with these parameters, we'd like to call
the action `resetpassword` of `Application\Controller\IndexController`.

> ### The zf command
>
> We will use `zf` to depict the entry point for your application; it can be
> a shell script in the application's `bin/` directory, or simply an alias for `php
> public/index.php`.

First we need to create a **route definition**:

```
user resetpassword <userEmail>
```

This simple route definition expects exactly three arguments: the literal
"user", the literal "resetpassword", and the dynamic argument we're calling
"userEmail". Let's assume we also accept one optional parameter to enable
verbose operation:

```
user resetpassword [--verbose|-v] <userEmail>
```

The modified console route above expects the same three arguments from our
original example, but will also recognise an optional `--verbose` flag, or its
shorthand version, `-v`.

> ### Flag order
>
> The order of flags is ignored by zend-console. Flags can appear before
> positional parameters, after them, or anywhere in between. The order of
> multiple flags is also irrelevant. This applies both to route definitions and
> the order that flags are used on the command line.

Let's use the definition above and configure our console route. When using
zend-mvc, console routes are defined using the following configuration
structure:

```php
return [
    'router' => [
        'routes' => [
            // HTTP routes are defined here
        ],
    ],

    'console' => [
        'router' => [
            'routes' => [
                // Console routes go here
            ],
        ],
    ],

    /* ... */
];
```

Let's create our console route and point it to
`Application\Controller\IndexController::resetpasswordAction()`:

```php
// We could define routes for Application\Controller\IndexController in
// the Application module config file, usually located at
// modules/application/config/module.config.php, or in one of the
// config/autoload/*.php files.
return [
    'console' => [
        'router' => [
            'routes' => [
                'user-reset-password' => [
                    'options' => [
                        'route'    => 'user resetpassword [--verbose|-v] <userEmail>',
                        'defaults' => [
                            'controller' => Application\Controller\Index::class,
                            'action'     => 'resetpassword',
                        ],
                    ],
                ],
            ],
        ],
    ],
    /* ... */
;
```

## Handling console requests

When a user runs our application from the command line and arguments match our
console route, the specified controller will be instantiated, and the specified
action method will be called, just as happens with zend-mvc HTTP requests.

As such, let's add the `resetpassword` action to our
`Application\Controller\IndexController`:

```php
<?php
namespace Application\Controller;

use RuntimeException;
use Zend\Mvc\Controller\AbstractActionController;
use Zend\View\Model\ViewModel;
use Zend\Console\Request as ConsoleRequest;
use Zend\Math\Rand;

class IndexController extends AbstractActionController
{
    /* ... */

    public function resetpasswordAction()
    {
        $request = $this->getRequest();

        // Make sure that we are running in a console, and the user has not
        // tricked our application into running this action from a public web
        // server:
        if (! $request instanceof ConsoleRequest) {
            throw new RuntimeException('You can only use this action from a console!');
        }

        // Get user email from the console, and check if the user requested
        // verbosity:
        $userEmail   = $request->getParam('userEmail');
        $verbose     = $request->getParam('verbose') || $request->getParam('v');

        // Reset new password
        $newPassword = Rand::getString(16);

        // Fetch the user and change his password, then email him ...
        /* ... */

        if ($verbose) {
            return "Done! New password for user $userEmail is '$newPassword'. "
                . "It has also been emailed to him.\n";
        }

        return "Done! $userEmail has received an email with his new password.\n";

    }
}
```

The above creates `resetpasswordAction()`, which:

- retrieves the current request;
- checks if it's really coming from the console (as a precaution); in this
  example, we do not want our action to be invocable from a web page;
- pulls console arguments via `$request->getParam()`; flags are represented by
  boolean values, where `true` means the flag was used, and `false` means
  otherwise;
- performs work based on the arguments;
- and finally returns a simple string to display to the user via the console.

## Adding console usage info

Console applications commonly display usage information when run without
arguments. The combination of zend-console and zend-mvc enables this out of the
box. Modules can provide usage information, and zend-console will query all loaded
modules for console usage information they expose.

Let's modify our `Application\Module` to provide usage info:

```php
<?php

namespace Application;

use Zend\ModuleManager\Feature\AutoloaderProviderInterface;
use Zend\ModuleManager\Feature\ConfigProviderInterface;
use Zend\ModuleManager\Feature\ConsoleUsageProviderInterface;
use Zend\Console\Adapter\AdapterInterface as Console;

/**
 * Implementing ConsoleUsageProviderInterface allows the Module to provide
 * console usage information.
 */
class Module implements
    AutoloaderProviderInterface,
    ConfigProviderInterface,
    ConsoleUsageProviderInterface
{
    public function getConfig()
    {
        /* ... */
    }

    public function getAutoloaderConfig()
    {
        /* ... */
    }

    public function getConsoleUsage(Console $console)
    {
        return [
            // Describe available commands
            'user resetpassword [--verbose|-v] EMAIL' => 'Reset password for a user',

            // Describe expected parameters
            [ 'EMAIL',        'Email of the user for a password reset' ],
            [ '--verbose|-v', '(optional) turn on verbose mode'        ],
        ];
    }
}
```

Each module that implements `ConsoleUsageProviderInterface` will be queried for
console usage info.  On route mismatch, all info from all modules will be
concatenated, formatted to console width, and shown to the user.

> ### Usage Order
>
> The order of usage info displayed in the console is in the same order in which
> modules load. If you want your application to display important usage info
> first, change the order your modules are loaded.
