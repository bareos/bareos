# FormDateTimeLocal

The `FormDateTimeLocal` view helper can be used to render a `<input
type="datetime-local">` HTML5 form input. It is meant to work with the
[DateTimeLocal element](../element/date-time-local.md), which provides a default
input specification for validating HTML5 datetime values.

`FormDateTimeLocal` extends from [FormDateTime](form-date-time.md).

## Basic usage

```php
use Zend\Form\Element;

$element = new Element\DateTimeLocal('my-datetime');

// Within your view...

echo $this->formDateTimeLocal($element);
// Result: <input type="datetime-local" name="my-datetime" value="">
```
