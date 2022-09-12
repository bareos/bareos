# View Helper - Links

## Introduction

The links helper is used for rendering *HTML* `LINK` elements. Links are used for describing
document relationships of the currently active page. Read more about links and link types at
[Document relationships: the LINK element (HTML4 W3C
Rec.)](http://www.w3.org/TR/html4/struct/links.html#h-12.3) and [Link types (HTML4 W3C
Rec.)](http://www.w3.org/TR/html4/types.html#h-6.12) in the *HTML*4 W3C Recommendation.

There are two types of relations; forward and reverse, indicated by the kewyords *'rel'* and
*'rev'*. Most methods in the helper will take a `$rel` param, which must be either *'rel'* or
*'rev'*. Most methods also take a `$type` param, which is used for specifying the link type (e.g.
alternate, start, next, prev, chapter, etc).

Relationships can be added to page objects manually, or found by traversing the container registered
in the helper. The method `findRelation($page, $rel, $type)` will first try to find the given `$rel`
of `$type` from the `$page` by calling *$page-&gt;findRel($type)* or *$page-&gt;findRel($type)*. If
the `$page` has a relation that can be converted to a page instance, that relation will be used. If
the `$page` instance doesn't have the specified `$type`, the helper will look for a method in the
helper named *search$rel$type* (e.g. `searchRelNext()` or `searchRevAlternate()`). If such a method
exists, it will be used for determining the `$page`'s relation by traversing the container.

Not all relations can be determined by traversing the container. These are the relations that will
be found by searching:

- `searchRelStart()`, forward 'start' relation: the first page in the container.
- `searchRelNext()`, forward 'next' relation; finds the next page in the container, i.e. the page
after the active page.
- `searchRelPrev()`, forward 'prev' relation; finds the previous page, i.e. the page before the
active page.
- `searchRelChapter()`, forward 'chapter' relations; finds all pages on level 0 except the 'start'
relation or the active page if it's on level 0.
- `searchRelSection()`, forward 'section' relations; finds all child pages of the active page if the
active page is on level 0 (a 'chapter').
- `searchRelSubsection()`, forward 'subsection' relations; finds all child pages of the active page
if the active pages is on level 1 (a 'section').
- `searchRevSection()`, reverse 'section' relation; finds the parent of the active page if the
active page is on level 1 (a 'section').
- `searchRevSubsection()`, reverse 'subsection' relation; finds the parent of the active page if the
active page is on level 2 (a 'subsection').

> ## Note
When looking for relations in the page instance (*$page-&gt;getRel($type)* or
*$page-&gt;getRev($type)*), the helper accepts the values of type `String`, `Array`, `Zend\Config`,
or `Zend\Navigation\Page\AbstractPage`. If a string is found, it will be converted to a
`Zend\Navigation\Page\Uri`. If an array or a config is found, it will be converted to one or several
page instances. If the first key of the array/config is numeric, it will be considered to contain
several pages, and each element will be passed to the page factory
&lt;zend.navigation.pages.factory&gt;. If the first key is not numeric, the array/config will be
passed to the page factory directly, and a single page will be returned.

The helper also supports magic methods for finding relations. E.g. to find forward alternate
relations, call *$helper-&gt;findRelAlternate($page)*, and to find reverse section relations, call
*$helper-&gt;findRevSection($page)*. Those calls correspond to *$helper-&gt;findRelation($page,
'rel', 'alternate');* and *$helper-&gt;findRelation($page, 'rev', 'section');* respectively.

To customize which relations should be rendered, the helper uses a render flag. The render flag is
an integer value, and will be used in a [bitwise and (&)
operation](http://php.net/manual/en/language.operators.bitwise.php) against the helper's render
constants to determine if the relation that belongs to the render constant should be rendered.

See the \[example below\](zend.navigation.view.helper.links.specify-rendering) for more information.

- `Zend\View\Helper\Navigation\Links::RENDER_ALTERNATE`
- `Zend\View\Helper\Navigation\Links::RENDER_STYLESHEET`
- `Zend\View\Helper\Navigation\Links::RENDER_START`
- `Zend\View\Helper\Navigation\Links::RENDER_NEXT`
- `Zend\View\Helper\Navigation\Links::RENDER_PREV`
- `Zend\View\Helper\Navigation\Links::RENDER_CONTENTS`
- `Zend\View\Helper\Navigation\Links::RENDER_INDEX`
- `Zend\View\Helper\Navigation\Links::RENDER_GLOSSARY`
- `Zend\View\Helper\Navigation\Links::RENDER_COPYRIGHT`
- `Zend\View\Helper\Navigation\Links::RENDER_CHAPTER`
- `Zend\View\Helper\Navigation\Links::RENDER_SECTION`
- `Zend\View\Helper\Navigation\Links::RENDER_SUBSECTION`
- `Zend\View\Helper\Navigation\Links::RENDER_APPENDIX`
- `Zend\View\Helper\Navigation\Links::RENDER_HELP`
- `Zend\View\Helper\Navigation\Links::RENDER_BOOKMARK`
- `Zend\View\Helper\Navigation\Links::RENDER_CUSTOM`
- `Zend\View\Helper\Navigation\Links::RENDER_ALL`

The constants from `RENDER_ALTERNATE` to `RENDER_BOOKMARK` denote standard *HTML* link types.
`RENDER_CUSTOM` denotes non-standard relations that specified in pages. `RENDER_ALL` denotes
standard and non-standard relations.

Methods in the links helper:

- *{get|set}RenderFlag()* gets/sets the render flag. Default is `RENDER_ALL`. See examples below on
how to set the render flag.
- `findAllRelations()` finds all relations of all types for a given page.
- `findRelation()` finds all relations of a given type from a given page.
- *searchRel{Start|Next|Prev|Chapter|Section|Subsection}()* traverses a container to find forward
relations to the start page, the next page, the previous page, chapters, sections, and subsections.
- *searchRev{Section|Subsection}()* traverses a container to find reverse relations to sections or
subsections.
- `renderLink()` renders a single *link* element.

## Basic usage

### Specify relations in pages

This example shows how to specify relations in pages.

```php
$container = new Zend\Navigation\Navigation(array(
    array(
        'label' => 'Relations using strings',
        'rel'   => array(
            'alternate' => 'http://www.example.org/'
        ),
        'rev'   => array(
            'alternate' => 'http://www.example.net/'
        )
    ),
    array(
        'label' => 'Relations using arrays',
        'rel'   => array(
            'alternate' => array(
                'label' => 'Example.org',
                'uri'   => 'http://www.example.org/'
            )
        )
    ),
    array(
        'label' => 'Relations using configs',
        'rel'   => array(
            'alternate' => new Zend\Config\Config(array(
                'label' => 'Example.org',
                'uri'   => 'http://www.example.org/'
            ))
        )
    ),
    array(
        'label' => 'Relations using pages instance',
        'rel'   => array(
            'alternate' => Zend\Navigation\Page\AbstractPage::factory(array(
                'label' => 'Example.org',
                'uri'   => 'http://www.example.org/'
            ))
        )
    )
));
```

### Default rendering of links

This example shows how to render a menu from a container registered/found in the view helper.

In a view script or layout:

```php
<?php echo $this->view->navigation()->links(); ?>
```

Output:

```php
<link rel="alternate" href="/products/server/faq/format/xml">
<link rel="start" href="/" title="Home">
<link rel="next" href="/products/server/editions" title="Editions">
<link rel="prev" href="/products/server" title="Foo Server">
<link rel="chapter" href="/products" title="Products">
<link rel="chapter" href="/company/about" title="Company">
<link rel="chapter" href="/community" title="Community">
<link rel="canonical" href="http://www.example.com/?page=server-faq">
<link rev="subsection" href="/products/server" title="Foo Server">
```

### Specify which relations to render

This example shows how to specify which relations to find and render.

Render only start, next, and prev:

```php
$helper->setRenderFlag(Zend\View\Helper\Navigation\Links::RENDER_START |
                       Zend\View\Helper\Navigation\Links::RENDER_NEXT |
                       Zend\View\Helper\Navigation\Links::RENDER_PREV);
```

Output:

```php
<link rel="start" href="/" title="Home">
<link rel="next" href="/products/server/editions" title="Editions">
<link rel="prev" href="/products/server" title="Foo Server">
```

Render only native link types:

```php
$helper->setRenderFlag(Zend\View\Helper\Navigation\Links::RENDER_ALL ^
                       Zend\View\Helper\Navigation\Links::RENDER_CUSTOM);
```

Output:

```php
<link rel="alternate" href="/products/server/faq/format/xml">
<link rel="start" href="/" title="Home">
<link rel="next" href="/products/server/editions" title="Editions">
<link rel="prev" href="/products/server" title="Foo Server">
<link rel="chapter" href="/products" title="Products">
<link rel="chapter" href="/company/about" title="Company">
<link rel="chapter" href="/community" title="Community">
<link rev="subsection" href="/products/server" title="Foo Server">
```

Render all but chapter:

```php
$helper->setRenderFlag(Zend\View\Helper\Navigation\Links::RENDER_ALL ^
                       Zend\View\Helper\Navigation\Links::RENDER_CHAPTER);
```

Output:

```php
<link rel="alternate" href="/products/server/faq/format/xml">
<link rel="start" href="/" title="Home">
<link rel="next" href="/products/server/editions" title="Editions">
<link rel="prev" href="/products/server" title="Foo Server">
<link rel="canonical" href="http://www.example.com/?page=server-faq">
<link rev="subsection" href="/products/server" title="Foo Server">
```
