# FormColor

The `FormColor` view helper can be used to render a `<input type="color">` HTML5
form input. It is meant to work with the [color element](../element/color.md).

## Basic usage

```php
use Zend\Form\Element;

$element = new Element\Color('my-color');

// Within your view...
echo $this->formColor($element);
```

Output:

```html
<input type="color" name="my-color" value="">
```
