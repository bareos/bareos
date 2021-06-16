# FormUrl

The `FormUrl` view helper can be used to render an `<input type="url">` HTML
form input. It is meant to work with the [Url element](../element/url.md), which
provides a default input specification with an URL validator.

## Basic usage

```php
use Zend\Form\Element;

$element = new Element\Url('my-url');

// Within your view...
echo $this->formUrl($element);
```

Output:

```html
<input type="url" name="my-url" value="">
```

Usage of custom regular expression pattern:

```php
use Zend\Form\Element;

$element = new Element\Url('my-url');
$element->setAttribute('pattern', 'https?://.+');

// Within your view...
echo $this->formUrl($element);
```

Output:

```html
<input type="url" name="my-url" pattern="https?://.+" value="">
```
