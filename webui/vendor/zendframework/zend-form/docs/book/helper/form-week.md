# FormWeek

The `FormWeek` view helper can be used to render an `<input type="week">` HTML5
form input. It is meant to work with the [Week element](../element/week.md),
which provides a default input specification for validating HTML5 week values.

`FormWeek` extends from [FormDateTime](form-date-time.md).

Basic usage:

```php
use Zend\Form\Element;

$element = new Element\Week('my-week');

// Within your view...
echo $this->formWeek($element);
```

Output:

```html
<input type="week" name="my-week" value="">
```

Usage of `min`, `max` and `step` attributes:

```php
use Zend\Form\Element;

$element = new Element\Week('my-week');
$element->setAttributes([
    'min'  => '2012-W01',
    'max'  => '2020-W01',
    'step' => 2, // weeks; default step interval is 1 week
]);
$element->setValue('2014-W10');

// Within your view...
echo $this->formWeek($element);
```

Output:

```html
<input type="week" name="my-week" min="2012-W01" max="2020-W01" step="2" value="2014-W10">
```
