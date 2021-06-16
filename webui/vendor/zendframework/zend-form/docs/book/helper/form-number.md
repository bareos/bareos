# FormNumber

The `FormNumber` view helper can be used to render an `<input type="number">`
HTML form input. It is meant to work with the [Number element](../element/number.md),
which provides a default input specification for validating numerical values.

## Basic usage

```php
use Zend\Form\Element;

$element = new Element\Number('my-number');

// Within your view...
echo $this->formNumber($element);
```

Output:

```html
<input type="number" name="my-number" value="">
```

Usage of `min`, `max` and `step` attributes:

```php
use Zend\Form\Element;

$element = new Element\Number('my-number');
$element->setAttributes([
    'min'  => 5,
    'max'  => 20,
    'step' => 0.5,
]);
$element->setValue(12);

// Within your view...
echo $this->formNumber($element);
```

Output:

```html
<input type="number" name="my-number" min="5" max="20" step="0.5" value="12">
```
