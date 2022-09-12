# Controller Plugins

When using the `AbstractActionController` or `AbstractRestfulController`, or if you implement the
`setPluginManager` method in your custom controllers, you have access to a number of pre-built
plugins. Additionally, you can register your own custom plugins with the manager.

The built-in plugins are:

-
\[Zend\\Mvc\\Controller\\Plugin\\AcceptableViewModelSelector\](zend.mvc.controller-plugins.acceptableviewmodelselector)
- \[Zend\\Mvc\\Controller\\Plugin\\FlashMessenger\](zend.mvc.controller-plugins.flashmessenger)
- \[Zend\\Mvc\\Controller\\Plugin\\Forward\](zend.mvc.controller-plugins.forward)
- \[Zend\\Mvc\\Controller\\Plugin\\Identity\](zend.mvc.controller-plugins.identity)
- \[Zend\\Mvc\\Controller\\Plugin\\Layout\](zend.mvc.controller-plugins.layout)
- \[Zend\\Mvc\\Controller\\Plugin\\Params\](zend.mvc.controller-plugins.params)
- \[Zend\\Mvc\\Controller\\Plugin\\PostRedirectGet\](zend.mvc.controller-plugins.postredirectget)
- \[Zend\\Mvc\\Controller\\Plugin\\Redirect\](zend.mvc.controller-plugins.redirect)
- \[Zend\\Mvc\\Controller\\Plugin\\Url\](zend.mvc.controller-plugins.url)

If your controller implements the `setPluginManager`, `getPluginManager` and `plugin` methods, you
can access these using their shortname via the `plugin()` method:

```php
$plugin = $this->plugin('url');
```

For an extra layer of convenience, both `AbstractActionController` and `AbstractRestfulController`
have `__call()` implementations that allow you to retrieve plugins via method calls:

```php
$plugin = $this->url();
```

## AcceptableViewModelSelector Plugin

The `AcceptableViewModelSelector` is a helper that can be used to select an appropriate view model
based on user defined criteria will be tested against the Accept header in the request.

As an example:

```php
use Zend\Mvc\Controller\AbstractActionController;
use Zend\View\Model\JsonModel;

class SomeController extends AbstractActionController
{
   protected $acceptCriteria = array(
      'Zend\View\Model\JsonModel' => array(
         'application/json',
      ),
      'Zend\View\Model\FeedModel' => array(
         'application/rss+xml',
      ),
   );

   public function apiAction()
   {
      $viewModel = $this->acceptableViewModelSelector($this->acceptCriteria);

      // Potentially vary execution based on model returned
      if ($viewModel instanceof JsonModel) {
         // ...
      }
   }
}
```

The above would return a standard `Zend\View\Model\ViewModel` instance if the criteria is not met,
and the specified view model types if the specific criteria is met. Rules are matched in order, with
the first match "winning."

## FlashMessenger Plugin

The `FlashMessenger` is a plugin designed to create and retrieve self-expiring, session-based
messages. It exposes a number of methods:

setSessionManager(Zend\\Session\\ManagerInterface $manager)

> Allows you to specify an alternate session manager, if desired.
rtype  
`Zend\Mvc\Controller\Plugin\FlashMessenger`
getSessionManager()

> Allows you to retrieve the session manager registered.
rtype  
`Zend\Session\ManagerInterface`
getContainer()

> Returns the `Zend\Session\Container` instance in which the flash messages are stored.
rtype  
`Zend\Session\Container`
setNamespace(string $namespace = 'default')

> Allows you to specify a specific namespace in the container in which to store or from which to
retrieve flash messages.
rtype  
`Zend\Mvc\Controller\Plugin\FlashMessenger`
- `getNamespace()` retrieves the name of the flash message namespace.

getNamespace()

> Retrieves the name of the flash message namespace.
rtype  
`string`
addMessage(string $message)

> Allows you to add a message to the current namespace of the session container.
rtype  
`Zend\Mvc\Controller\Plugin\FlashMessenger`
hasMessages()

> Lets you determine if there are any flash messages from the current namespace in the session
container.
rtype  
`boolean`
getMessages()

> Retrieves the flash messages from the current namespace of the session container
rtype  
`array`
clearMessages()

> Clears all flash messages in current namespace of the session container. Returns `true` if
messages were cleared, `false` if none existed.
rtype  
`boolean`
hasCurrentMessages()

> Indicates whether any messages were added during the current request.
rtype  
`boolean`
getCurrentMessages()

> Retrieves any messages added during the current request.
rtype  
`array`
clearCurrentMessages()

> Removes any messages added during the current request. Returns `true` if current messages were
cleared, `false` if none existed.
rtype  
`boolean`
clearMessagesFromContainer()

> Clear all messages from the container. Returns `true` if any messages were cleared, `false` if
none existed.
rtype  
`boolean`
This plugin also provides four meaningful namespaces, namely: INFO, ERROR, WARNING, SUCCESS. The
following functions are related to these namespaces:

addInfoMessage()

> Add a message to "info" namespace

> rtype  
`Zend\Mvc\Controller\Plugin\FlashMessenger`
hasCurrentInfoMessages()

> Check to see if messages have been added to "info" namespace within this request

> rtype  
`boolean`
addWarningMessage()

> Add a message to "warning" namespace

> rtype  
`Zend\Mvc\Controller\Plugin\FlashMessenger`
hasCurrentWarningMessages()

> Check to see if messages have been added to "warning" namespace within this request

> rtype  
`boolean`
addErrorMessage()

> Add a message to "error" namespace

> rtype  
`Zend\Mvc\Controller\Plugin\FlashMessenger`
hasCurrentErrorMessages()

> Check to see if messages have been added to "error" namespace within this request

> rtype  
`boolean`
addSuccessMessage()

> Add a message to "success" namespace

> rtype  
`Zend\Mvc\Controller\Plugin\FlashMessenger`
hasCurrentSuccessMessages()

> Check to see if messages have been added to "success" namespace within this request

> rtype  
`boolean`
Additionally, the `FlashMessenger` implements both `IteratorAggregate` and `Countable`, allowing you
to iterate over and count the flash messages in the current namespace within the session container.

**Examples**

```php
public function processAction()
{
    // ... do some work ...
    $this->flashMessenger()->addMessage('You are now logged in.');
    return $this->redirect()->toRoute('user-success');
}

public function successAction()
{
    $return = array('success' => true);
    $flashMessenger = $this->flashMessenger();
    if ($flashMessenger->hasMessages()) {
        $return['messages'] = $flashMessenger->getMessages();
    }
    return $return;
}
```

## Forward Plugin

Occasionally, you may want to dispatch additional controllers from within the matched controller --
for instance, you might use this approach to build up "widgetized" content. The `Forward` plugin
helps enable this.

For the `Forward` plugin to work, the controller calling it must be `ServiceLocatorAware`;
otherwise, the plugin will be unable to retrieve a configured and injected instance of the requested
controller.

The plugin exposes a single method, `dispatch()`, which takes two arguments:

- `$name`, the name of the controller to invoke. This may be either the fully qualified class name,
or an alias defined and recognized by the `ServiceManager` instance attached to the invoking
controller.
- `$params` is an optional array of parameters with which to seed a `RouteMatch` object for purposes
of this specific request. Meaning the parameters will be matched by their key to the routing
identifiers in the config (otherwise non-matching keys are ignored)

`Forward` returns the results of dispatching the requested controller; it is up to the developer to
determine what, if anything, to do with those results. One recommendation is to aggregate them in
any return value from the invoking controller.

As an example:

```php
$foo = $this->forward()->dispatch('foo', array('action' => 'process'));
return array(
    'somekey' => $somevalue,
    'foo'     => $foo,
);
```

## Identity Plugin

The `Identity` plugin allows for getting the identity from the `AuthenticationService`.

For the `Identity` plugin to work, a `Zend\Authentication\AuthenticationService` name or alias must
be defined and recognized by the `ServiceManager`.

`Identity` returns the identity in the `AuthenticationService` or null if no identity is available.

As an example:

```php
public function testAction()
{
    if ($user = $this->identity()) {
         // someone is logged !
    } else {
         // not logged in
    }
}
```

When invoked, the `Identity` plugin will look for a service by the name or alias
`Zend\Authentication\AuthenticationService` in the `ServiceManager`. You can provide this service to
the `ServiceManager` in a configuration file:

```php
// In a configuration file...
return array(
    'service_manager' => array(
        'aliases' => array(
            'Zend\Authentication\AuthenticationService' => 'my_auth_service',
        ),
        'invokables' => array(
            'my_auth_service' => 'Zend\Authentication\AuthenticationService',
        ),
    ),
);
```

The `Identity` plugin exposes two methods:

setAuthenticationService(Zend\\Authentication\\AuthenticationService $authenticationService)

> Sets the authentication service instance to be used by the plugin.
rtype  
`void`
getAuthenticationService()

> Retrieves the current authentication service instance if any is attached.
rtype  
`Zend\Authentication\AuthenticationService`
## Layout Plugin

The `Layout` plugin allows for changing layout templates from within controller actions.

It exposes a single method, `setTemplate()`, which takes one argument:

- `$template`, the name of the template to set.

As an example:

```php
$this->layout()->setTemplate('layout/newlayout');
```

It also implements the `__invoke` magic method, which allows for even easier setting of the
template:

```php
$this->layout('layout/newlayout');
```

## Params Plugin

The `Params` plugin allows for accessing parameters in actions from different sources.

It exposes several methods, one for each parameter source:

fromFiles(string $name = null, mixed $default = null)

> For retrieving all or one single **file**. If `$name` is null, all files will be returned.
rtype  
`array|ArrayAccess|null`
fromHeader(string $header = null, mixed $default = null)

> For retrieving all or one single **header** parameter. If `$header` is null, all header parameters
will be returned.
rtype  
`null|Zend\Http\Header\HeaderInterface`
fromPost(string $param = null, mixed $default = null)

> For retrieving all or one single **post** parameter. If `$param` is null, all post parameters will
be returned.
rtype  
`mixed`
fromQuery(string $param = null, mixed $default = null)

> For retrieving all or one single **query** parameter. If `$param` is null, all query parameters
will be returned.
rtype  
`mixed`
fromRoute(string $param = null, mixed $default = null)

> For retrieving all or one single **route** parameter. If `$param` is null, all route parameters
will be returned.
rtype  
`mixed`
It also implements the `__invoke` magic method, which allows for short circuiting to the `fromRoute`
method:

```php
$this->params()->fromRoute('param', $default);
// or
$this->params('param', $default);
```

## Post/Redirect/Get Plugin

When a user sends a POST request (e.g. after submitting a form), their browser will try to protect
them from sending the POST again, breaking the back button, causing browser warnings and pop-ups,
and sometimes reposting the form. Instead, when receiving a POST, we should store the data in a
session container and redirect the user to a GET request.

This plugin can be invoked with two arguments:

- `$redirect`, a string containing the redirect location which can either be a named route or a URL,
based on the contents of the second parameter.
- `$redirectToUrl`, a boolean that when set to TRUE, causes the first parameter to be treated as a
URL instead of a route name (this is required when redirecting to a URL instead of a route). This
argument defaults to false.

When no arguments are provided, the current matched route is used.

**Example Usage**

```php
// Pass in the route/url you want to redirect to after the POST
$prg = $this->prg('/user/register', true);

if ($prg instanceof \Zend\Http\PhpEnvironment\Response) {
    // returned a response to redirect us
    return $prg;
} elseif ($prg === false) {
    // this wasn't a POST request, but there were no params in the flash messenger
    // probably this is the first time the form was loaded
    return array('form' => $myForm);
}

// $prg is an array containing the POST params from the previous request
$form->setData($prg);

// ... your form processing code here
```

## File Post/Redirect/Get Plugin

While similar to the standard \[Post/Redirect/Get
Plugin\](zend.mvc.controller-plugins.postredirectget), the File PRG Plugin will work for forms with
file inputs. The difference is in the behavior: The File PRG Plugin will interact directly with your
form instance and the file inputs, rather than *only* returning the POST params from the previous
request.

By interacting directly with the form, the File PRG Plugin will turn off any file inputs' `required`
flags for already uploaded files (for a partially valid form state), as well as run the file input
filters to move the uploaded files into a new location (configured by the user).

> ## Warning
You **must** attach a Filter for moving the uploaded files to a new location, such as the
\[RenameUpload Filter\](zend.filter.file.rename-upload), or else your files will be removed upon the
redirect.

This plugin can be invoked with three arguments:

- `$form`: the form instance.
- `$redirect`: (Optional) a string containing the redirect location which can either be a named
route or a URL, based on the contents of the third parameter. If this argument is not provided, it
will default to the current matched route.
- `$redirectToUrl`: (Optional) a boolean that when set to TRUE, causes the second parameter to be
treated as a URL instead of a route name (this is required when redirecting to a URL instead of a
route). This argument defaults to false.

**Example Usage**

```php
$myForm = new Zend\Form\Form('my-form');
$myForm->add(array(
    'type' => 'Zend\Form\Element\File',
    'name' => 'file',
));
// NOTE: Without a filter to move the file,
//       our files will disappear between the requests
$myForm->getInputFilter()->getFilterChain()->attach(
    new Zend\Filter\File\RenameUpload(array(
        'target'    => './data/tmpuploads/file',
        'randomize' => true,
    ))
);

// Pass in the form and optional the route/url you want to redirect to after the POST
$prg = $this->fileprg($myForm, '/user/profile-pic', true);

if ($prg instanceof \Zend\Http\PhpEnvironment\Response) {
    // Returned a response to redirect us
    return $prg;
} elseif ($prg === false) {
    // First time the form was loaded
    return array('form' => $myForm);
}

// Form was submitted.
// $prg is now an array containing the POST params from the previous request,
// but we don't have to apply it to the form since that has already been done.

// Process the form
if ($form->isValid()) {
    // ...Save the form...
    return $this->redirect()->toRoute('/user/profile-pic/success');
} else {
    // Form not valid, but file uploads might be valid and uploaded
    $fileErrors = $form->get('file')->getMessages();
    if (empty($fileErrors)) {
        $tempFile = $form->get('file')->getValue();
    }
}
```

## Redirect Plugin

Redirections are quite common operations within applications. If done manually, you will need to do
the following steps:

- Assemble a url using the router
- Create and inject a "Location" header into the `Response` object, pointing to the assembled URL
- Set the status code of the `Response` object to one of the 3xx HTTP statuses.

The `Redirect` plugin does this work for you. It offers three methods:

toRoute(string $route = null, array $params = array(), array $options = array(), boolean
$reuseMatchedParams = false)

> Redirects to a named route, using the provided `$params` and `$options` to assembled the URL.
rtype  
`Zend\Http\Response`
toUrl(string $url)

> Simply redirects to the given URL.
rtype  
`Zend\Http\Response`
refresh()

> Refresh to current route
rtype  
`Zend\Http\Response`
In each case, the `Response` object is returned. If you return this immediately, you can effectively
short-circuit execution of the request.

> ## Note
This plugin requires that the controller invoking it implements `InjectApplicationEventInterface`,
and thus has an `MvcEvent` composed, as it retrieves the router from the event object.

As an example:

```php
return $this->redirect()->toRoute('login-success');
```

## Url Plugin

Often you may want to generate URLs from route definitions within your controllers -- in order to
seed the view, generate headers, etc. While the `MvcEvent` object composes the router, doing so
manually would require this workflow:

```php
$router = $this->getEvent()->getRouter();
$url    = $router->assemble($params, array('name' => 'route-name'));
```

The `Url` helper makes this slightly more convenient:

```php
$url = $this->url()->fromRoute('route-name', $params);
```

The `fromRoute()` method is the only public method defined, and has the following signature:

> ## Note
This plugin requires that the controller invoking it implements `InjectApplicationEventInterface`,
and thus has an `MvcEvent` composed, as it retrieves the router from the event object.
