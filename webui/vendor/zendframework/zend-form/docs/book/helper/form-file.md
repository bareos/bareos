# FormFile

The `FormFile` view helper can be used to render an `<input type="file">` form
input. It is meant to work with the [File element](../element/file.md).

## Basic usage

```php
use Zend\Form\Element;

$element = new Element\File('my-file');

// Within your view...

echo $this->formFile($element);
// Result: <input type="file" name="my-file">
```

For HTML5 multiple file uploads, the `multiple` attribute can be used. Browsers
that do not support HTML5 will default to a single upload input.

```php
use Zend\Form\Element;

$element = new Element\File('my-file');
$element->setAttribute('multiple', true);

// Within your view...

echo $this->formFile($element);
// Result: <input type="file" name="my-file" multiple="multiple">
```
