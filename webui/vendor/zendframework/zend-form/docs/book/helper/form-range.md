# FormRange

The `FormRange` view helper can be used to render an `<input type="range">` HTML
form input. It is meant to work with the [Range element](../element/range.md),
which provides a default input specification for validating numerical ranges.

## Basic usage

```php
use Zend\Form\Element;

$element = new Element\Range('my-range');

// Within your view...
echo $this->formRange($element);
```

Output:

```html
<input type="range" name="my-range" value="">
```

Usage of `min`, `max` and `step` attributes:

```php
use Zend\Form\Element;

$element = new Element\Range('my-range');
$element->setAttributes([
    'min'  => 0,
    'max'  => 100,
    'step' => 5,
]);
$element->setValue(20);

// Within your view...
echo $this->formRange($element);
```

Output:

```html
<input type="range" name="my-range" min="0" max="100" step="5" value="20">
```
