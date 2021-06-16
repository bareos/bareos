# FormDateTime

The `FormDateTime` view helper can be used to render a `<input type="datetime">`
HTML5 form input. It is meant to work with the [DateTime element](../element/date-time.md),
which provides a default input specification for validating HTML5 datetime
values.

## Basic usage

```php
use Zend\Form\Element;

$element = new Element\DateTime('my-datetime');

// Within your view...

echo $this->formDateTime($element);
// Result: <input type="datetime" name="my-datetime" value="">
```
