# Pages

`Zend\Navigation` ships with two page types:

- \[MVC pages\](zend.navigation.pages.mvc) – using the class `Zend\Navigation\Page\Mvc`
- \[URI pages\](zend.navigation.pages.uri) – using the class `Zend\Navigation\Page\Uri`

MVC pages are link to on-site web pages, and are defined using MVC parameters (*action*,
*controller*, *route*, *params*). URI pages are defined by a single property *uri*, which give you
the full flexibility to link off-site pages or do other things with the generated links (e.g. an URI
that turns into `<a href="#">foo<a>`).

orphan  

## Common page features

All page classes must extend `Zend\Navigation\Page\AbstractPage`, and will thus share a common set
of features and properties. Most notably they share the options in the table below and the same
initialization process.

Option keys are mapped to *set* methods. This means that the option *order* maps to the method
`setOrder()`, and *reset\_params* maps to the method `setResetParams()`. If there is no setter
method for the option, it will be set as a custom property of the page.

Read more on extending `Zend\Navigation\Page\AbstractPage` in Creating custom page types
&lt;zend.navigation.pages.custom&gt;.

> ## Note
#### Custom properties
All pages support setting and getting of custom properties by use of the magic methods `__set($name,
$value)`, `__get($name)`, `__isset($name)` and `__unset($name)`. Custom properties may have any
value, and will be included in the array that is returned from `$page-toArray()`, which means that
pages can be serialized/deserialized successfully even if the pages contains properties that are not
native in the page class.
Both native and custom properties can be set using `$page-set($name, $value)` and retrieved using
`$page-get($name)`, or by using magic methods.

### Custom page properties

This example shows how custom properties can be used.

```php
$page = new Zend\Navigation\Page\Mvc();
$page->foo     = 'bar';
$page->meaning = 42;

echo $page->foo;

if ($page->meaning != 42) {
    // action should be taken
}
```

orphan  

## Zend\\Navigation\\Page\\Mvc

*MVC* pages are defined using *MVC* parameters known from the `Zend\Mvc` component. An *MVC* page
will use `Zend\Mvc\Router\RouteStackInterface` internally in the `getHref()` method to generate
hrefs, and the `isActive()` method will compare the `Zend\Mvc\Router\RouteMatch` params with the
page's params to determine if the page is active.

> ## Note
Starting in version 2.2.0, if you want to re-use any matched route parameters when generating a
link, you can do so via the "useRouteMatch" flag. This is particularly useful when creating segment
routes that include the currently selected language or locale as an initial segment, as it ensures
the links generated all include the matched value.

> ## Note
The *URI* returned is relative to the *baseUrl* in `Zend\Mvc\Router\Http\TreeRouteStack`. In the
examples, the baseUrl is '/' for simplicity.

### getHref() generates the page URI

This example show that *MVC* pages use `Zend\Mvc\Router\RouteStackInterface` internally to generate
*URI*s when calling *$page-&gt;getHref()*.

```php
// Create route
$route = Zend\Mvc\Router\Http\Segment::factory(array(
   'route'       => '/[:controller[/:action][/:id]]',
   'constraints' => array(
      'controller' => '[a-zA-Z][a-zA-Z0-9_-]+',
      'action'     => '[a-zA-Z][a-zA-Z0-9_-]+',
      'id'         => '[0-9]+',
   ),
   array(
      'controller' => 'Album\Controller\Album',
      'action'     => 'index',
   )
));
$router = new Zend\Mvc\Router\Http\TreeRouteStack();
$router->addRoute('default', $route);

// getHref() returns /album/add
$page = new Zend\Navigation\Page\Mvc(array(
    'action'     => 'add',
    'controller' => 'album',
));

// getHref() returns /album/edit/1337
$page = new Zend\Navigation\Page\Mvc(array(
    'action'     => 'edit',
    'controller' => 'album',
    'params'     => array('id' => 1337),
));

 // getHref() returns /album/1337?format=json
$page = new Zend\Navigation\Page\Mvc(array(
    'action'     => 'edit',
    'controller' => 'album',
    'params'     => array('id' => 1337),
    'query'      => array('format' => 'json'),
));
```

### isActive() determines if page is active

This example show that *MVC* pages determine whether they are active by using the params found in
the route match object.

```php
/**
 * Dispatched request:
 * - controller: album
 * - action:     index
 */
$page1 = new Zend\Navigation\Page\Mvc(array(
    'action'     => 'index',
    'controller' => 'album',
));

$page2 = new Zend\Navigation\Page\Mvc(array(
    'action'     => 'edit',
    'controller' => 'album',
));

$page1->isActive(); // returns true
$page2->isActive(); // returns false

/**
 * Dispatched request:
 * - controller: album
 * - action:     edit
 * - id:         1337
 */
$page = new Zend\Navigation\Page\Mvc(array(
    'action'     => 'edit',
    'controller' => 'album',
    'params'     => array('id' => 1337),
));

// returns true, because request has the same controller and action
$page->isActive();

/**
 * Dispatched request:
 * - controller: album
 * - action:     edit
 */
$page = new Zend\Navigation\Page\Mvc(array(
    'action'     => 'edit',
    'controller' => 'album',
    'params'     => array('id' => null),
));

// returns false, because page requires the id param to be set in the request
$page->isActive(); // returns false
```

### Using routes

Routes can be used with *MVC* pages. If a page has a route, this route will be used in `getHref()`
to generate the *URL* for the page.

> ## Note
Note that when using the *route* property in a page, you do not need to specify the default params
that the route defines (controller, action, etc.).

```php
// the following route is added to the ZF router
$route = Zend\Mvc\Router\Http\Segment::factory(array(
   'route'       => '/a/:id',
   'constraints' => array(
      'id' => '[0-9]+',
   ),
   array(
      'controller' => 'Album\Controller\Album',
      'action'     => 'show',
   )
));
$router = new Zend\Mvc\Router\Http\TreeRouteStack();
$router->addRoute('albumShow', $route);

// a page is created with a 'route' option
$page = new Zend\Navigation\Page\Mvc(array(
    'label'      => 'Show album',
    'route'      => 'albumShow',
    'params'     => array('id' => 42)
));

// returns: /a/42
$page->getHref();
```

orphan  

## Zend\\Navigation\\Page\\Uri

Pages of type `Zend\Navigation\Page\Uri` can be used to link to pages on other domains or sites, or
to implement custom logic for the page. *URI* pages are simple; in addition to the common page
options, a *URI* page takes only one option — *uri*. The *uri* will be returned when calling
`$page->getHref()`, and may be a `String` or `NULL`.

> ## Note
`Zend\Navigation\Page\Uri` will not try to determine whether it should be active when calling
`$page-isActive()`. It merely returns what currently is set, so to make a *URI* page active you have
to manually call `$page-setActive()` or specifying *active* as a page option when constructing.

orphan  

## Creating custom page types

When extending `Zend\Navigation\Page\AbstractPage`, there is usually no need to override the
constructor or the methods `setOptions()` or `setConfig()`. The page constructor takes a single
parameter, an `Array` or a `Zend\Config` object, which is passed to `setOptions()` or `setConfig()`
respectively. Those methods will in turn call `set()` method, which will map options to native or
custom properties. If the option `internal_id` is given, the method will first look for a method
named `setInternalId()`, and pass the option to this method if it exists. If the method does not
exist, the option will be set as a custom property of the page, and be accessible via `$internalId =
$page->internal_id;` or `$internalId = $page->get('internal_id');`.

### The most simple custom page

The only thing a custom page class needs to implement is the `getHref()` method.

```php
class My\Simple\Page extends Zend\Navigation\Page\AbstractPage
{
    public function getHref()
    {
        return 'something-completely-different';
    }
}
```

### A custom page with properties

When adding properties to an extended page, there is no need to override/modify `setOptions()` or
`setConfig()`.

```php
class My\Navigation\Page extends Zend\Navigation\Page\AbstractPage
{
    protected $foo;
    protected $fooBar;

    public function setFoo($foo)
    {
        $this->foo = $foo;
    }

    public function getFoo()
    {
        return $this->foo;
    }

    public function setFooBar($fooBar)
    {
        $this->fooBar = $fooBar;
    }

    public function getFooBar()
    {
        return $this->fooBar;
    }

    public function getHref()
    {
        return $this->foo . '/' . $this->fooBar;
    }
}

// can now construct using
$page = new My\Navigation\Page(array(
    'label'   => 'Property names are mapped to setters',
    'foo'     => 'bar',
    'foo_bar' => 'baz'
));

// ...or
$page = Zend\Navigation\Page\AbstractPage::factory(array(
    'type'    => 'My\Navigation\Page',
    'label'   => 'Property names are mapped to setters',
    'foo'     => 'bar',
    'foo_bar' => 'baz'
));
```

orphan  

## Creating pages using the page factory

All pages (also custom classes), can be created using the page factory,
`Zend\Navigation\Page\AbstractPage::factory()`. The factory can take an array with options, or a
`Zend\Config` object. Each key in the array/config corresponds to a page option, as seen in the
section on \[Pages\](zend.navigation.pages). If the option *uri* is given and no *MVC* options are
given (*action, controller, route*), an *URI* page will be created. If any of the *MVC* options are
given, an *MVC* page will be created.

If *type* is given, the factory will assume the value to be the name of the class that should be
created. If the value is *mvc* or *uri* and *MVC*/URI page will be created.

### Creating an MVC page using the page factory

```php
$page = Zend\Navigation\Page\AbstractPage::factory(array(
    'label'  => 'My MVC page',
    'action' => 'index',
));

$page = Zend\Navigation\Page\AbstractPage::factory(array(
    'label'      => 'Search blog',
    'action'     => 'index',
    'controller' => 'search',
));

$page = Zend\Navigation\Page\AbstractPage::factory(array(
    'label' => 'Home',
    'route' => 'home',
));

$page = Zend\Navigation\Page\AbstractPage::factory(array(
    'type'   => 'mvc',
    'label'  => 'My MVC page',
));
```

### Creating a URI page using the page factory

```php
$page = Zend\Navigation\Page\AbstractPage::factory(array(
    'label' => 'My URI page',
    'uri'   => 'http://www.example.com/',
));

$page = Zend\Navigation\Page\AbstractPage::factory(array(
    'label'  => 'Search',
    'uri'    => 'http://www.example.com/search',
    'active' => true,
));

$page = Zend\Navigation\Page\AbstractPage::factory(array(
    'label' => 'My URI page',
    'uri'   => '#',
));

$page = Zend\Navigation\Page\AbstractPage::factory(array(
    'type'  => 'uri',
    'label' => 'My URI page',
));
```

### Creating a custom page type using the page factory

To create a custom page type using the factory, use the option *type* to specify a class name to
instantiate.

```php
class My\Navigation\Page extends Zend\Navigation\Page\AbstractPage
{
    protected $_fooBar = 'ok';

    public function setFooBar($fooBar)
    {
        $this->_fooBar = $fooBar;
    }
}

$page = Zend\Navigation\Page\AbstractPage::factory(array(
    'type'    => 'My\Navigation\Page',
    'label'   => 'My custom page',
    'foo_bar' => 'foo bar',
));
```
