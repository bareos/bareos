# View Helpers

## Introduction

The navigation helpers are used for rendering navigational elements from
Zend\\\\Navigation\\\\Navigation
&lt;zend.navigation.containers&gt; instances.

There are 5 built-in helpers:

- \[Breadcrumbs\](zend.navigation.view.helper.breadcrumbs), used for rendering the path to the
currently active page.
- \[Links\](zend.navigation.view.helper.links), used for rendering navigational head links (e.g.
`<link rel="next" href="..." />`)
- \[Menu\](zend.navigation.view.helper.menu), used for rendering menus.
- \[Sitemap\](zend.navigation.view.helper.sitemap), used for rendering sitemaps conforming to the
[Sitemaps XML format](http://www.sitemaps.org/protocol.php).
- \[Navigation\](zend.navigation.view.helper.navigation), used for proxying calls to other
navigational helpers.

All built-in helpers extend `Zend\View\Helper\Navigation\AbstractHelper`, which adds integration
with ACL
&lt;zend.permissions.acl&gt; and \[translation\](zend.i18n.translating). The abstract class
implements the interface `Zend\View\Helper\Navigation\HelperInterface`, which defines the following
methods:

- `getContainer()` and `setContainer()` gets and sets the navigation container the helper should
operate on by default, and `hasContainer()` checks if the helper has container registered.
- `getTranslator()` and `setTranslator()` gets and sets the translator used for translating labels
and titles. `isTranslatorEnabled()` and `setTranslatorEnabled()` controls whether the translator
should be enabled. The method `hasTranslator()` checks if the helper has a translator registered.
- `getAcl()`, `setAcl()`, `getRole()` and `setRole()`, gets and sets *ACL*
(`Zend\Permissions\Acl\AclInterface`) instance and role (`String` or
`Zend\Permissions\Acl\Role\RoleInterface`) used for filtering out pages when rendering.
`getUseAcl()` and `setUseAcl()` controls whether *ACL* should be enabled. The methods `hasAcl()` and
`hasRole()` checks if the helper has an *ACL* instance or a role registered.
- `__toString()`, magic method to ensure that helpers can be rendered by echoing the helper instance
directly.
- `render()`, must be implemented by concrete helpers to do the actual rendering.

In addition to the method stubs from the interface, the abstract class also implements the following
methods:

- `getIndent()` and `setIndent()` gets and sets indentation. The setter accepts a `String` or an
`Integer`. In the case of an `Integer`, the helper will use the given number of spaces for
indentation. I.e., `setIndent(4)` means 4 initial spaces of indentation. Indentation can be
specified for all helpers except the Sitemap helper.
- `getMinDepth()` and `setMinDepth()` gets and sets the minimum depth a page must have to be
included by the helper. Setting `NULL` means no minimum depth.
- `getMaxDepth()` and `setMaxDepth()` gets and sets the maximum depth a page can have to be included
by the helper. Setting `NULL` means no maximum depth.
- `getRenderInvisible()` and `setRenderInvisible()` gets and sets whether to render items that have
been marked as invisible or not.
- `__call()` is used for proxying calls to the container registered in the helper, which means you
can call methods on a helper as if it was a container. See example
    &lt;zend.navigation.view.helpers.proxy.example&gt; below.
- `findActive($container, $minDepth, $maxDepth)` is used for finding the deepest active page in the
given container. If depths are not given, the method will use the values retrieved from
`getMinDepth()` and `getMaxDepth()`. The deepest active page must be between `$minDepth` and
`$maxDepth` inclusively. Returns an array containing a reference to the found page instance and the
depth at which the page was found.
- `htmlify()` renders an **'a'** *HTML* element from a `Zend\Navigation\Page\AbstractPage` instance.
- `accept()` is used for determining if a page should be accepted when iterating containers. This
method checks for page visibility and verifies that the helper's role is allowed access to the
page's resource and privilege.
- The static method `setDefaultAcl()` is used for setting a default *ACL* object that will be used
by helpers.
- The static method `setDefaultRole()` is used for setting a default *Role* that will be used by
helpers

If a container is not explicitly set, the helper will create an empty `Zend\Navigation\Navigation`
container when calling `$helper->getContainer()`.

### Proxying calls to the navigation container

Navigation view helpers use the magic method `__call()` to proxy method calls to the navigation
container that is registered in the view helper.

```php
$this->navigation()->addPage(array(
    'type' => 'uri',
    'label' => 'New page'));
```

The call above will add a page to the container in the `Navigation` helper.

## Translation of labels and titles

The navigation helpers support translation of page labels and titles. You can set a translator of
type `Zend\I18n\Translator` in the helper using `$helper->setTranslator($translator)`.

If you want to disable translation, use `$helper->setTranslatorEnabled(false)`.

The \[proxy helper\](zend.navigation.view.helper.navigation) will inject its own translator to the
helper it proxies to if the proxied helper doesn't already have a translator.

> ## Note
There is no translation in the sitemap helper, since there are no page labels or titles involved in
an *XML* sitemap.

## Integration with ACL

All navigational view helpers support *ACL* inherently from the class
`Zend\View\Helper\Navigation\AbstractHelper`. An object implementing
`Zend\Permissions\Acl\AclInterface` can be assigned to a helper instance with
*$helper-&gt;setAcl($acl)*, and role with *$helper-&gt;setRole('member')* or
*$helper-&gt;setRole(new Zend\\Permissions\\Acl\\Role\\GenericRole('member'))*. If *ACL* is used in
the helper, the role in the helper must be allowed by the *ACL* to access a page's *resource* and/or
have the page's *privilege* for the page to be included when rendering.

If a page is not accepted by *ACL*, any descendant page will also be excluded from rendering.

The \[proxy helper\](zend.navigation.view.helper.navigation) will inject its own *ACL* and role to
the helper it proxies to if the proxied helper doesn't already have any.

The examples below all show how *ACL* affects rendering.

## Navigation setup used in examples

This example shows the setup of a navigation container for a fictional software company.

Notes on the setup:

- The domain for the site is *www.example.com*.
- Interesting page properties are marked with a comment.
- Unless otherwise is stated in other examples, the user is requesting the *URL*
*<http://www.example.com/products/server/faq/*>, which translates to the page labeled `FAQ`
under*Foo Server\*.
- The assumed *ACL* and router setup is shown below the container setup.

```php
/*
 * Navigation container (config/array)

 * Each element in the array will be passed to
 * Zend\Navigation\Page\AbstractPage::factory() when constructing
 * the navigation container below.
 */
$pages = array(
    array(
        'label'      => 'Home',
        'title'      => 'Go Home',
        'module'     => 'default',
        'controller' => 'index',
        'action'     => 'index',
        'order'      => -100 // make sure home is the first page
    ),
    array(
        'label'      => 'Special offer this week only!',
        'module'     => 'store',
        'controller' => 'offer',
        'action'     => 'amazing',
        'visible'    => false // not visible
    ),
    array(
        'label'      => 'Products',
        'module'     => 'products',
        'controller' => 'index',
        'action'     => 'index',
        'pages'      => array(
            array(
                'label'      => 'Foo Server',
                'module'     => 'products',
                'controller' => 'server',
                'action'     => 'index',
                'pages'      => array(
                    array(
                        'label'      => 'FAQ',
                        'module'     => 'products',
                        'controller' => 'server',
                        'action'     => 'faq',
                        'rel'        => array(
                            'canonical' => 'http://www.example.com/?page=faq',
                            'alternate' => array(
                                'module'     => 'products',
                                'controller' => 'server',
                                'action'     => 'faq',
                                'params'     => array('format' => 'xml')
                            )
                        )
                    ),
                    array(
                        'label'      => 'Editions',
                        'module'     => 'products',
                        'controller' => 'server',
                        'action'     => 'editions'
                    ),
                    array(
                        'label'      => 'System Requirements',
                        'module'     => 'products',
                        'controller' => 'server',
                        'action'     => 'requirements'
                    )
                )
            ),
            array(
                'label'      => 'Foo Studio',
                'module'     => 'products',
                'controller' => 'studio',
                'action'     => 'index',
                'pages'      => array(
                    array(
                        'label'      => 'Customer Stories',
                        'module'     => 'products',
                        'controller' => 'studio',
                        'action'     => 'customers'
                    ),
                    array(
                        'label'      => 'Support',
                        'module'     => 'products',
                        'controller' => 'studio',
                        'action'     => 'support'
                    )
                )
            )
        )
    ),
    array(
        'label'      => 'Company',
        'title'      => 'About us',
        'module'     => 'company',
        'controller' => 'about',
        'action'     => 'index',
        'pages'      => array(
            array(
                'label'      => 'Investor Relations',
                'module'     => 'company',
                'controller' => 'about',
                'action'     => 'investors'
            ),
            array(
                'label'      => 'News',
                'class'      => 'rss', // class
                'module'     => 'company',
                'controller' => 'news',
                'action'     => 'index',
                'pages'      => array(
                    array(
                        'label'      => 'Press Releases',
                        'module'     => 'company',
                        'controller' => 'news',
                        'action'     => 'press'
                    ),
                    array(
                        'label'      => 'Archive',
                        'route'      => 'archive', // route
                        'module'     => 'company',
                        'controller' => 'news',
                        'action'     => 'archive'
                    )
                )
            )
        )
    ),
    array(
        'label'      => 'Community',
        'module'     => 'community',
        'controller' => 'index',
        'action'     => 'index',
        'pages'      => array(
            array(
                'label'      => 'My Account',
                'module'     => 'community',
                'controller' => 'account',
                'action'     => 'index',
                'resource'   => 'mvc:community.account' // resource
            ),
            array(
                'label' => 'Forums',
                'uri'   => 'http://forums.example.com/',
                'class' => 'external' // class
            )
        )
    ),
    array(
        'label'      => 'Administration',
        'module'     => 'admin',
        'controller' => 'index',
        'action'     => 'index',
        'resource'   => 'mvc:admin', // resource
        'pages'      => array(
            array(
                'label'      => 'Write new article',
                'module'     => 'admin',
                'controller' => 'post',
                'action'     => 'write'
            )
        )
    )
);

// Create container from array
$container = new Zend\Navigation\Navigation($pages);

// Store the container in the proxy helper:
$view->plugin('navigation')->setContainer($container);

// ...or simply:
$view->navigation($container);
```

In addition to the container above, the following setup is assumed:

```php
<?php
// module/MyModule/config/module.config.php

return array(
    /* ... */
    'router' array(
        'routes' => array(
            'archive' => array(
                'type'    => 'Segment',
                'options' => array(
                    'route'    => '/archive/:year',
                    'defaults' => array(
                        'module'     => 'company',
                        'controller' => 'news',
                        'action'     => 'archive',
                        'year'       => (int) date('Y') - 1,
                    ),
                    'constraints' => array(
                        'year' => '\d+',
                    ),
                ),
            ),
            /* You can have other routes here... */
        ),
    ),
    /* ... */
);
```

```php
<?php
// module/MyModule/Module.php

namespace MyModule;

use Zend\View\HelperPluginManager;
use Zend\Permissions\Acl\Acl;
use Zend\Permissions\Acl\Role\GenericRole;
use Zend\Permissions\Acl\Resource\GenericResource;

class Module
{
    /* ... */
    public function getViewHelperConfig()
    {
        return array(
            'factories' => array(
                // This will overwrite the native navigation helper
                'navigation' => function(HelperPluginManager $pm) {
                    // Setup ACL:
                    $acl = new Acl();
                    $acl->addRole(new GenericRole('member'));
                    $acl->addRole(new GenericRole('admin'));
                    $acl->addResource(new GenericResource('mvc:admin'));
                    $acl->addResource(new GenericResource('mvc:community.account'));
                    $acl->allow('member', 'mvc:community.account');
                    $acl->allow('admin', null);

                    // Get an instance of the proxy helper
                    $navigation = $pm->get('Zend\View\Helper\Navigation');

                    // Store ACL and role in the proxy helper:
                    $navigation->setAcl($acl)
                               ->setRole('member');

                    // Return the new navigation helper instance
                    return $navigation;
                }
            )
        );
    }
    /* ... */
}
```
