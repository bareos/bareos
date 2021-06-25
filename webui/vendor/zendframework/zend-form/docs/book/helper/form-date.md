# FormDate

The `FormDate` view helper can be used to render a `<input type="date">` HTML5
form input. It is meant to work with the [date element](../element/date.md),
which provides a default input specification for validating HTML5 date values.

`FormDate` extends from the [FormDateTime helper](form-date-time.md).

## Basic usage

```php
use Zend\Form\Element;

$element = new Element\Date('my-date');

// Within your view...

echo $this->formDate($element);
// Result: <input type="date" name="my-date" value="">
```
