# FormImage

The `FormImage` view helper can be used to render an `<input type="image">` HTML form input. It is meant to work with the [Image element](../element/image.md).

## Basic usage

```php
use Zend\Form\Element;

$element = new Element\Image('my-image');
$element->setAttribute('src', '/img/my-pic.png');

// Within your view...

echo $this->formImage($element);
// Result: <input type="image" name="my-image" src="/img/my-pic.png">
```
