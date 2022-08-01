# View Helper - Breadcrumbs

## Introduction

Breadcrumbs are used for indicating where in a sitemap a user is currently browsing, and are
typically rendered like this: "You are here: Home &gt; Products &gt; FantasticProduct 1.0". The
breadcrumbs helper follows the guidelines from [Breadcrumbs Pattern - Yahoo! Design Pattern
Library](http://developer.yahoo.com/ypatterns/pattern.php?pattern=breadcrumbs), and allows simple
customization (minimum/maximum depth, indentation, separator, and whether the last element should be
linked), or rendering using a partial view script.

The Breadcrumbs helper works like this; it finds the deepest active page in a navigation container,
and renders an upwards path to the root. For *MVC* pages, the "activeness" of a page is determined
by inspecting the request object, as stated in the section on
\[Zend\\Navigation\\Page\\Mvc\](zend.navigation.pages.mvc).

The helper sets the *minDepth* property to 1 by default, meaning breadcrumbs will not be rendered if
the deepest active page is a root page. If *maxDepth* is specified, the helper will stop rendering
when at the specified depth (e.g. stop at level 2 even if the deepest active page is on level 3).

Methods in the breadcrumbs helper:

- *{get|set}Separator()* gets/sets separator string that is used between breadcrumbs. Default is *'
&gt; '*.
- *{get|set}LinkLast()* gets/sets whether the last breadcrumb should be rendered as an anchor or
not. Default is `FALSE`.
- *{get|set}Partial()* gets/sets a partial view script that should be used for rendering
breadcrumbs. If a partial view script is set, the helper's `render()` method will use the
`renderPartial()` method. If no partial is set, the `renderStraight()` method is used. The helper
expects the partial to be a `String` or an `Array` with two elements. If the partial is a `String`,
it denotes the name of the partial script to use. If it is an `Array`, the first element will be
used as the name of the partial view script, and the second element is the module where the script
is found.
- `renderStraight()` is the default render method.
- `renderPartial()` is used for rendering using a partial view script.

## Basic usage

This example shows how to render breadcrumbs with default settings.

In a view script or layout:

```php
<?php echo $this->navigation()->breadcrumbs(); ?>
```

The two calls above take advantage of the magic \_\_toString() method, and are equivalent to:

```php
<?php echo $this->navigation()->breadcrumbs()->render(); ?>
```

Output:

```php
<a href="/products">Products</a> &gt; <a href="/products/server">Foo Server</a> &gt; FAQ
```

## Specifying indentation

This example shows how to render breadcrumbs with initial indentation.

Rendering with 8 spaces indentation:

```php
<?php echo $this->navigation()->breadcrumbs()->setIndent(8);?>
```

Output:

```php
<a href="/products">Products</a> &gt; <a href="/products/server">Foo Server</a> &gt; FAQ
```

## Customize output

This example shows how to customize breadcrumbs output by specifying various options.

In a view script or layout:

```php
<?php
echo $this->navigation()
          ->breadcrumbs()
          ->setLinkLast(true)                   // link last page
          ->setMaxDepth(1)                      // stop at level 1
          ->setSeparator(' ▶' . PHP_EOL); // cool separator with newline
?>
```

Output:

```php
<a href="/products">Products</a> ▶
<a href="/products/server">Foo Server</a>
```

Setting minimum depth required to render breadcrumbs:

```php
<?php
$this->navigation()->breadcrumbs()->setMinDepth(10);
echo $this->navigation()->breadcrumbs();
?>
```

Output:

Nothing, because the deepest active page is not at level 10 or deeper.

## Rendering using a partial view script

This example shows how to render customized breadcrumbs using a partial vew script. By calling
`setPartial()`, you can specify a partial view script that will be used when calling `render()`.
When a partial is specified, the `renderPartial()` method will be called. This method will find the
deepest active page and pass an array of pages that leads to the active page to the partial view
script.

In a layout:

```php
echo $this->navigation()->breadcrumbs()
                        ->setPartial('my-module/partials/breadcrumbs');
```

Contents of *module/MyModule/view/my-module/partials/breadcrumbs.phtml*:

```php
echo implode(', ', array_map(
        function ($a) { return $a->getLabel(); },
        $this->pages));
```

Output:

```php
Products, Foo Server, FAQ
```
