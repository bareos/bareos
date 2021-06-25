# HtmlTag

The `HtmlTag` helper is used to **create the root of an HTML document**, the
open and close tags for the `<html>` element.

## Basic Usage

```php
<?= $this->htmlTag(['lang' => 'en'])->openTag() ?>
<!-- Some HTML -->
<?= $this->htmlTag()->closeTag() ?>
```

Output:

```html
<html lang="en">
<!-- Some HTML -->
</html>
```

## Using Attributes

### Set a single Attribute

```php fct_label="Invoke Usage"
$this->htmlTag(['lang' => 'en']);

echo $this->htmlTag()->openTag(); // <html lang="en">
```

```php fct_label="Setter Usage"
$this->htmlTag()->setAttribute('lang', 'en');

echo $this->htmlTag()->openTag(); // <html lang="en">
```

### Set multiple Attributes

```php fct_label="Invoke Usage"
$this->htmlTag(['lang' => 'en', 'id' => 'example']);

echo $this->htmlTag()->openTag(); // <html lang="en" id="example">
```

```php fct_label="Setter Usage"
$this->htmlTag()->setAttributes(['lang' => 'en', 'id' => 'example']);

echo $this->htmlTag()->openTag(); // <html lang="en" id="example">
```

### Get current Value

To get the current value, use the `getAttributes()` method.

```php
$this->htmlTag(['lang' => 'en', 'id' => 'example']);

var_dump($this->htmlTag()->getAttributes()); // ['lang' => 'en', 'id' => 'example']
```

### Default Value

The default value is an empty `array` that means no attributes are set.

## Using Namespace

The `HtmlTag` helper can automatically add the [XHTML namespace](http://www.w3.org/1999/xhtml/)
for XHTML documents. To use this functionality, the [`Doctype` helper](doctype.md)
is used.

The namespace is added only if the document type is set to an XHTML type and use
is enabled:

```php
// Set doctype to XHTML
$this->doctype(Zend\View\Helper\Doctype::XHTML1_STRICT);

// Add namespace to open tag
$this->htmlTag()->setUseNamespaces(true);

// Output
echo $this->htmlTag()->openTag(); // <html xmlns="http://www.w3.org/1999/xhtml">
```

### Get current Value

To get the current value, use the `getUseNamespaces()` method.

```php
$this->htmlTag()->setUseNamespaces(true);

var_dump($this->htmlTag()->getUseNamespaces()); // true
```

### Default Value

The default value is `false` that means no namespace is added as attribute.
