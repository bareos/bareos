# FormText

The `FormText` view helper can be used to render an `<input type="text">` HTML
form input. It is meant to work with the [Text element](../element/text.md).

## Basic usage

```php
use Zend\Form\Element;

$element = new Element\Text('my-text');

// Within your view...
echo $this->formText($element);
```

Output:

```html
<input type="text" name="my-text" value="">
```
