# FormTime

The `FormTime` view helper can be used to render an `<input type="time">` HTML5
form input. It is meant to work with the [Time element](../element/time.md),
which provides a default input specification for validating HTML5 time values.

`FormTime` extends from [FormDateTime](form-date-time.md).

## Basic usage

```php
use Zend\Form\Element;

$element = new Element\Time('my-time');

// Within your view...
echo $this->formTime($element);
// Result: <input type="time" name="my-time" value="">
```
