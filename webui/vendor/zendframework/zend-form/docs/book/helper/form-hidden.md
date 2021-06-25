# FormHidden

The `FormHidden` view helper can be used to render an `<input type="hidden">`
HTML form input. It is meant to work with the [Hidden element](../element/hidden.md).

## Basic usage

```php
use Zend\Form\Element;

$element = new Element\Hidden('my-hidden');
$element->setValue('foo');

// Within your view...

echo $this->formHidden($element);
// Result: <input type="hidden" name="my-hidden" value="foo">
```
