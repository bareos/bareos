# DateTime

`Zend\Form\Element\DateTime` is meant to be paired with the
[FormDateTime](../helper/form-date-time.md) helper for
[HTML5 inputs with type "datetime"](http://www.whatwg.org/specs/web-apps/current-work/multipage/states-of-the-type-attribute.html#date-and-time-state-%28type=datetime%29).
This element adds filters and validators to its input filter specification in
order to validate HTML5 datetime input values on the server.

## Basic Usage

This element automatically adds a `type` attribute of value `datetime`.

```php
use Zend\Form\Element;
use Zend\Form\Form;

$dateTime = new Element\DateTime('appointment-date-time');
$dateTime->setLabel('Appointment Date/Time');
$dateTime->setAttributes([
    'min'  => '2010-01-01T00:00:00Z',
    'max'  => '2020-01-01T00:00:00Z',
    'step' => '1', // minutes; default step interval is 1 min
]);
$dateTime->setOptions([
    'format' => 'Y-m-d\TH:iP'
]);

$form = new Form('my-form');
$form->add($dateTime);
```

Using array notation:

```php
use Zend\Form\Element;
use Zend\Form\Form;

$form = new Form('my-form');
$form->add([
	'type' => Element\DateTime::class,
	'name' => 'appointment-date-time',
	'options' => [
		'label' => 'Appointment Date/Time',
		'format' => 'Y-m-d\TH:iP',
	],
	'attributes' => [
		'min' => '2010-01-01T00:00:00Z',
		'max' => '2020-01-01T00:00:00Z',
		'step' => '1', // minutes; default step interval is 1 min
	],
]);
```

> ### Set all attributes before calling prepare
>
> The `min`, `max`, and `step` attributes should be set prior to calling
> `Zend\Form::prepare()`. Otherwise, the default input specification for the
> element may not contain the correct validation rules.

## Public Methods

The following methods are specific to the `DateTime` element; all other methods
defined by the [parent `Element` class](element.md#public-methods) are also
available.

Method signature                    | Description
----------------------------------- | -----------
`getInputSpecification() : array`   | See write-up below.
`setOptions(array $options) : void` | Set options for an element of type `DateTime`. The accepted option, in addition to the inherited options of [`Zend\Form\Element`](element.md#public-methods) is `format`, which calls `setFormat()`.
`setFormat(string $format) : void`  | Sets the format used to validate the value. Accepts a PHP `DateTime` compatible string.
`getFormat() : string`              | Return the PHP `DateTime` format used to validate the value.

`getInputSpecification()` returns an input filter specification, which includes
the `StringTrim` filter, and appropriate validators based on the values from the
`min`, `max`, and `step` attributes, as well as the `format` option, per the
following:

- If the `min` attribute is set, a `Zend\Validator\GreaterThan` validator will
  be added to ensure the date value is greater than the minimum value.
- If the `max` attribute is set, a `Zend\Validator\LessThanValidator` validator
  will be added to ensure the date value is less than the maximum value.
- If the `step` attribute is set to "any", step validations will be skipped.
  Otherwise, a `Zend\Validator\DateStep` validator will be added to ensure the
  date value is within a certain interval of minutes (default is 1 minute).
- The input filter specification also includes a `Zend\Validator\Date` validator
  to ensure the format of the value. If the `format` option is set, that format
  will be used. Otherwise the default format will be used.
