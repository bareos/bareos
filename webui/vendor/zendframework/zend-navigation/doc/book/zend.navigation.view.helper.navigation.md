# View Helper - Navigation Proxy

## Introduction

The Navigation helper is a proxy helper that relays calls to other navigational helpers. It can be
considered an entry point to all navigation-related view tasks. The aforementioned navigational
helpers are in the namespace `Zend\View\Helper\Navigation`, and would thus require the path
*Zend/View/Helper/Navigation* to be added as a helper path to the view. With the proxy helper
residing in the `Zend\View\Helper` namespace, it will always be available, without the need to add
any helper paths to the view.

The Navigation helper finds other helpers that implement the
`Zend\View\Helper\Navigation\HelperInterface`, which means custom view helpers can also be proxied.
This would, however, require that the custom helper path is added to the view.

When proxying to other helpers, the Navigation helper can inject its container, *ACL*/role, and
translator. This means that you won't have to explicitly set all three in all navigational helpers,
nor resort to injecting by means of static methods.

## Methods

- `findHelper()` finds the given helper, verifies that it is a navigational helper, and injects
container, *ACL*/role and translator.
- *{get|set}InjectContainer()* gets/sets a flag indicating whether the container should be injected
to proxied helpers. Default is `TRUE`.
- *{get|set}InjectAcl()* gets/sets a flag indicating whether the *ACL*/role should be injected to
proxied helpers. Default is `TRUE`.
- *{get|set}InjectTranslator()* gets/sets a flag indicating whether the translator should be
injected to proxied helpers. Default is `TRUE`.
- *{get|set}DefaultProxy()* gets/sets the default proxy. Default is *'menu'*.
- `render()` proxies to the render method of the default proxy.

