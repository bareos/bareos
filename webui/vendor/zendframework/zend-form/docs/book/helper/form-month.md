# FormMonth

The `FormMonth` view helper can be used to render an `<input type="month">`
HTML5 form input. It is meant to work with the [Month element](../element/month.md),
which provides a default input specification for validating HTML5 date values.

`FormMonth` extends from [FormDateTime](form-date-time.md).

## Basic usage

```php
use Zend\Form\Element;

$element = new Element\Month('my-month');

// Within your view...

echo $this->formMonth($element);
// Result: <input type="month" name="my-month" value="">
```
