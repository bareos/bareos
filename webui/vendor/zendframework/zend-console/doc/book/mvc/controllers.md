# Console-aware controllers

When using the zend-mvc integration with zend-console, a matched route results
in dispatch of an action controller. In this chapter we will learn how ZF2
Controllers can interact with and return output to console window.

## Handling console requests

Console requests are very similar to HTTP requests. In fact, they implement a
common interface and are created at the same time in the MVC workflow. [Console
routes](../routes.md) match against command line arguments and provide a `defaults`
array, which holds the `controller` and `action` keys. These correspond with
controller aliases in the `ServiceManager`, and method names in the controller
class. This is analogous to the way HTTP requests are handled under zend-mvc.

In this example we'll use the following route:

```php
// in file module/Application/config/module.config.php:
return [
    'router' => [
        'routes' => [
            // HTTP routes are here
        ],
    ],

    'console' => [
        'router' => [
            'routes' => [
                'list-users' => [
                    'options' => [
                        'route'    => 'show [all|disabled|deleted]:mode users [--verbose|-v]',
                        'defaults' => [
                            'controller' => Application\Controller\Index::class,
                            'action'     => 'show-users',
                        ],
                    ],
                ],
            ],
        ],
    ],

    /* ... */
);
```

This route will match commands such as:

```bash
$ php public/index.php show users
$ php public/index.php show all users
$ php public/index.php show disabled users
```

This route maps to the method `Application\Controller\IndexController::showUsersAction()`.

Let's add it that method to our controller.

```php
<?php
namespace Application\Controller;

use Zend\Mvc\Controller\AbstractActionController;
use Zend\View\Model\ViewModel;

class IndexController extends AbstractActionController
{
    public function indexAction()
    {
        return new ViewModel(); // display standard index page
    }

    public function showUsersAction()
    {
        $request = $this->getRequest();
        $users   = $this->getServiceLocator()->get('users');

        // Check verbose flag
        $verbose = $request->getParam('verbose') || $request->getParam('v');

        // Check mode
        $mode = $request->getParam('mode', 'all'); // defaults to 'all'

        $users = array();
        switch ($mode) {
            case 'disabled':
                $users = $users->fetchDisabledUsers();
                break;
            case 'deleted':
                $users = $users->fetchDeletedUsers();
                break;
            case 'all':
            default:
                $users = $users->fetchAllUsers();
                break;
        }
    }
}
```

We fetch the console request, read parameters, and load users from our
(theoretical) users service.  In order to make this method functional, we'll
have to display the result in the console window.

## Sending output to the console

The simplest way for our controller to display data in the console window is to
return a string. Let's modify our example to output a list of users:

```php
public function showUsersAction()
{
    $request = $this->getRequest();
    $users   = $this->getServiceLocator()->get('users');

    // Check verbose flag
    $verbose = $request->getParam('verbose') || $request->getParam('v');

    // Check mode
    $mode = $request->getParam('mode', 'all'); // defaults to 'all'

    $users = array();
    switch ($mode) {
        case 'disabled':
            $users = $users->fetchDisabledUsers();
            break;
        case 'deleted':
            $users = $users->fetchDeletedUsers();
            break;
        case 'all':
        default:
            $users = $users->fetchAllUsers();
            break;
    }

    if (count($users) === 0) {
        // Show an error message in the console
        return "There are no users in the database\n";
    }

    $result = '';

    foreach ($users as $user) {
        $result .= $user->name . ' ' . $user->email . "\n";
    }

    return $result; // show it in the console
}
```

In the second conditional, we are checking if the users service found any users;
if not, we return an error message to display immediately, terminating the
application.

If any users are found, we loop through each to prepare a listing, which we then
return from the action for display in the console.

## Are we in a console?

Sometimes we might need to check if our method is being called from a console or
from a web request. This is useful to block certain methods from running in the
console or to change their behavior based on that context.

Here is an example of how to check if we are dealing with a console request:

```php
namespace Application\Controller;

use Zend\Mvc\Controller\AbstractActionController;
use Zend\View\Model\ViewModel;
use Zend\Console\Request as ConsoleRequest;
use RuntimeException;

class IndexController extends AbstractActionController
{
    public function showUsersAction()
    {
        $request = $this->getRequest();

        // Make sure that we are running in a console and the user has not tricked our
        // application into running this action from a public web server.
        if (! $request instanceof ConsoleRequest) {
            throw new RuntimeException('You can only use this action from a console!');
        }

        /* ... */
    }
}
```

> ### Use routing to protect methods
>
> You do not need to secure all your controllers and methods from console
> requests. Controller actions will **only be invoked** when at least one
> console route matches it.  HTTP and Console routes are separated and defined
> in different places in module (and application) configuration.
>
> There is no way to invoke a console action unless there is at least one route
> pointing to it.  Similarly, there is no way for an HTTP action to be invoked
> unless there is at least one HTTP route that points to it.

The example below shows how a single controller method can handle **both Console
and HTTP requests**:

```php
namespace Application\Controller;

use Zend\Mvc\Controller\AbstractActionController;
use Zend\View\Model\ViewModel;
use Zend\Console\Request as ConsoleRequest;
use Zend\Http\Request as HttpRequest;
use RuntimeException;

class IndexController extends AbstractActionController
{
    public function showUsersAction()
    {
        $request = $this->getRequest();

        $users = array();
        /* ... fetch users from database ... */

        if ($request instanceof HttpRequest) {
            // display a web page with users list
            return new ViewModel($result);
        }
        
        if ($request instanceof ConsoleRequest) {
            // ... prepare console output and return it ...
            return $result;
        }

        throw new RuntimeException('Cannot handle request of type ' . get_class($request));
    }
}
```

> ### AbstractConsoleController
> 
> One way to ensure you always receive a console request instance is to extend
> `Zend\Mvc\Controller\AbstractConsoleController`. This controller instance also
> exposes a new method, `getConsole()`, providing you access to the console
> adapter, allowing you to use prompts, send output (including colorized output),
> and more.

## Reading values from console parameters

There are several types of parameters recognized by the Console component, all
of which are described in the [console routing chapter](../routes.md). Here, we'll
focus on how to retrieve values from distinct parameters and flags.

### Positional parameters

After a route matches, we can access both **literal parameters** and **value
parameters** via the `$request` instance, using the `getParam()` method.

Assuming we have the following route:

```php
'show-users' => [
    'options' => [
        'route'    => 'show (all|deleted|locked|admin) [<groupName>]'
        'defaults' => [
            'controller' => 'Application\Controller\Users',
            'action'     => 'showusers',
        ],
    ],
],
```

If this route matches, our action can now query parameters in the following way:

```php
// an action inside Application\Controller\UsersController:
public function showUsersAction()
{
    $request = $this->getRequest();

    // We can access named value parameters directly by their name:
    $showUsersFromGroup = $request->getParam('groupName');

    // Literal parameters can be checked with isset() against their exact spelling
    if (isset($request->getParam('all'))) {
        // show all users
    } elseif (isset($request->getParam('deleted'))) {
        // show deleted users
    }

    /* ... */
}
```

In case of parameter alternatives, it is a good idea to **assign a name to the group**, which
simplifies the branching in our action controllers. We can do this with the following syntax:

```php
// inside of config.console.router.routes:
'show-users' => array(
    'options' => array(
        'route'    => 'show (all|deleted|locked|admin):userTypeFilter [<groupName>]'
        'defaults' => array(
            'controller' => 'Application\Controller\Users',
            'action'     => 'showusers'
        )
    )
)
```

Now we can use a the group name `userTypeFilter` to check which option has been
selected by the user:

```php
public function showUsersAction()
{
    $request = $this->getRequest();

    // We can access named value parameters directly by their name:
    $showUsersFromGroup = $request->getParam('groupName');

    // The selected option from second parameter is now stored under 'userTypeFilter'
    $userTypeFilter = $request->getParam('userTypeFilter');

    switch ($userTypeFilter) {
        case 'all':
            // all users
        case 'deleted':
            // deleted users
        case 'locked'
           // ...
           // ...
    }
}
```

### Flags

Flags are directly accessible by name. Value-capturing flags will contain string
values, as provided by the user. Non-value flags will be equal to `true`, or
`null` if not present.

Given the following route:

```php
'find-user' => [
    'options' => [
        'route'    => 'find user [--fast] [--verbose] [--id=] [--firstName=] [--lastName=] [--email=] ',
        'defaults' => [
            'controller' => 'Application\Controller\Users',
            'action'     => 'find',
        ],
    ],
],
```

We can retrieve values in the following fashion:

```php
public function findAction()
{
    $request = $this->getRequest();

    // We can retrieve values from value flags using their name
    $searchId        = $request->getParam('id',        null); // default null
    $searchFirstName = $request->getParam('firstName', null);
    $searchLastName  = $request->getParam('lastName',  null);
    $searchEmail     = $request->getParam('email',     null);

    // Standard flags that have been matched will be equal to TRUE
    $isFast          = (bool) $request->getParam('fast',    false); // default false
    $isVerbose       = (bool) $request->getParam('verbose', false);

    if ($isFast) {
        // perform a fast query ...
    } else {
        // perform standard query ...
    }
}
```

In case of **flag alternatives**, we have to check each alternative separately:

```php
/*
 * Assuming our route now reads:
 *      'route'    => 'find user [--fast|-f] [--verbose|-v] ... ',
 */
public function findAction()
{
    $request = $this->getRequest();

    // Check both alternatives
    $isFast    = $request->getParam('fast', false)    || $request->getParam('f', false);
    $isVerbose = $request->getParam('verbose', false) || $request->getParam('v', false);

    // ...
}
```
