# View Helper - Sitemap

## Introduction

The Sitemap helper is used for generating *XML* sitemaps, as defined by the [Sitemaps XML
format](http://www.sitemaps.org/protocol.php). Read more about [Sitemaps on
Wikipedia](http://en.wikipedia.org/wiki/Sitemaps).

By default, the sitemap helper uses \[sitemap validators\](zend.validator.sitemap) to validate each
element that is rendered. This can be disabled by calling
*$helper-&gt;setUseSitemapValidators(false)*.

> ## Note
If you disable sitemap validators, the custom properties (see table) are not validated at all.

The sitemap helper also supports [Sitemap XSD
Schema](http://www.sitemaps.org/schemas/sitemap/0.9/sitemap.xsd) validation of the generated
sitemap. This is disabled by default, since it will require a request to the Schema file. It can be
enabled with *$helper-&gt;setUseSchemaValidation(true)*.

Methods in the sitemap helper:

- *{get|set}FormatOutput()* gets/sets a flag indicating whether *XML* output should be formatted.
This corresponds to the *formatOutput* property of the native `DOMDocument` class. Read more at
[PHP: DOMDocument - Manual](http://php.net/domdocument). Default is `FALSE`.
- *{get|set}UseXmlDeclaration()* gets/sets a flag indicating whether the *XML* declaration should be
included when rendering. Default is `TRUE`.
- *{get|set}UseSitemapValidators()* gets/sets a flag indicating whether sitemap validators should be
used when generating the DOM sitemap. Default is `TRUE`.
- *{get|set}UseSchemaValidation()* gets/sets a flag indicating whether the helper should use *XML*
Schema validation when generating the DOM sitemap. Default is `FALSE`. If `TRUE`.
- *{get|set}ServerUrl()* gets/sets server *URL* that will be prepended to non-absolute *URL*s in the
`url()` method. If no server *URL* is specified, it will be determined by the helper.
- `url()` is used to generate absolute *URL*s to pages.
- `getDomSitemap()` generates a DOMDocument from a given container.

## Basic usage

This example shows how to render an *XML* sitemap based on the setup we did further up.

```php
// In a view script or layout:

// format output
$this->navigation()
      ->sitemap()
      ->setFormatOutput(true); // default is false

// other possible methods:
// ->setUseXmlDeclaration(false); // default is true
// ->setServerUrl('http://my.otherhost.com');
// default is to detect automatically

// print sitemap
echo $this->navigation()->sitemap();
```

Notice how pages that are invisible or pages with *ACL* roles incompatible with the view helper are
filtered out:

```php
<?xml version="1.0" encoding="UTF-8"?>
<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">
  <url>
    <loc>http://www.example.com/</loc>
  </url>
  <url>
    <loc>http://www.example.com/products</loc>
  </url>
  <url>
    <loc>http://www.example.com/products/server</loc>
  </url>
  <url>
    <loc>http://www.example.com/products/server/faq</loc>
  </url>
  <url>
    <loc>http://www.example.com/products/server/editions</loc>
  </url>
  <url>
    <loc>http://www.example.com/products/server/requirements</loc>
  </url>
  <url>
    <loc>http://www.example.com/products/studio</loc>
  </url>
  <url>
    <loc>http://www.example.com/products/studio/customers</loc>
  </url>
  <url>
    <loc>http://www.example.com/products/studio/support</loc>
  </url>
  <url>
    <loc>http://www.example.com/company/about</loc>
  </url>
  <url>
    <loc>http://www.example.com/company/about/investors</loc>
  </url>
  <url>
    <loc>http://www.example.com/company/news</loc>
  </url>
  <url>
    <loc>http://www.example.com/company/news/press</loc>
  </url>
  <url>
    <loc>http://www.example.com/archive</loc>
  </url>
  <url>
    <loc>http://www.example.com/community</loc>
  </url>
  <url>
    <loc>http://www.example.com/community/account</loc>
  </url>
  <url>
    <loc>http://forums.example.com/</loc>
  </url>
</urlset>
```

## Rendering using no *ACL* role

Render the sitemap using no *ACL* role (should filter out /community/account):

```php
echo $this->navigation()
          ->sitemap()
          ->setFormatOutput(true)
          ->setRole();
```

```php
<?xml version="1.0" encoding="UTF-8"?>
<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">
  <url>
    <loc>http://www.example.com/</loc>
  </url>
  <url>
    <loc>http://www.example.com/products</loc>
  </url>
  <url>
    <loc>http://www.example.com/products/server</loc>
  </url>
  <url>
    <loc>http://www.example.com/products/server/faq</loc>
  </url>
  <url>
    <loc>http://www.example.com/products/server/editions</loc>
  </url>
  <url>
    <loc>http://www.example.com/products/server/requirements</loc>
  </url>
  <url>
    <loc>http://www.example.com/products/studio</loc>
  </url>
  <url>
    <loc>http://www.example.com/products/studio/customers</loc>
  </url>
  <url>
    <loc>http://www.example.com/products/studio/support</loc>
  </url>
  <url>
    <loc>http://www.example.com/company/about</loc>
  </url>
  <url>
    <loc>http://www.example.com/company/about/investors</loc>
  </url>
  <url>
    <loc>http://www.example.com/company/news</loc>
  </url>
  <url>
    <loc>http://www.example.com/company/news/press</loc>
  </url>
  <url>
    <loc>http://www.example.com/archive</loc>
  </url>
  <url>
    <loc>http://www.example.com/community</loc>
  </url>
  <url>
    <loc>http://forums.example.com/</loc>
  </url>
</urlset>
```

## Rendering using a maximum depth

Render the sitemap using a maximum depth of 1.

```php
echo $this->navigation()
          ->sitemap()
          ->setFormatOutput(true)
          ->setMaxDepth(1);
```

```php
<?xml version="1.0" encoding="UTF-8"?>
<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">
  <url>
    <loc>http://www.example.com/</loc>
  </url>
  <url>
    <loc>http://www.example.com/products</loc>
  </url>
  <url>
    <loc>http://www.example.com/products/server</loc>
  </url>
  <url>
    <loc>http://www.example.com/products/studio</loc>
  </url>
  <url>
    <loc>http://www.example.com/company/about</loc>
  </url>
  <url>
    <loc>http://www.example.com/company/about/investors</loc>
  </url>
  <url>
    <loc>http://www.example.com/company/news</loc>
  </url>
  <url>
    <loc>http://www.example.com/community</loc>
  </url>
  <url>
    <loc>http://www.example.com/community/account</loc>
  </url>
  <url>
    <loc>http://forums.example.com/</loc>
  </url>
</urlset>
```

> ## Note
#### UTF-8 encoding used by default
By default, Zend Framework uses *UTF-8* as its default encoding, and, specific to this case,
`Zend\View` does as well. So if you want to use another encoding with `Sitemap`, you will have do
three things:
1.  Create a custom renderer and implement a `getEncoding()` method;
2.  Create a custom rendering strategy that will return an instance of your custom renderer;
3.  Attach the custom strategy in the `ViewEvent`;
See the \[example from HeadStyle
documentation\](zend.view.helpers.initial.headstyle.encoding.example) to see how you can achieve
this.
