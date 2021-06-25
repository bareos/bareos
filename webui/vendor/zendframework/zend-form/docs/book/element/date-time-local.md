# DateTimeLocal

`Zend\Form\Element\DateTimeLocal` is meant to be paired with the
[FormDateTimeLocal](../helper/form-date-time-local.md) helper for
[HTML5 inputs with type "datetime-local"](http://www.whatwg.org/specs/web-apps/current-work/multipage/states-of-the-type-attribute.html#local-date-and-time-state-%28type=datetime-local%29).
This element adds filters and validators to its input filter specification in
order to validate a local datetime input value on the server.

## Basic Usage

This element automatically adds a `type` attribute of value `datetime-local`.

```php
use Zend\Form\Element;
use Zend\Form\Form;

$dateTimeLocal = new Element\DateTimeLocal('appointment-date-time');
$dateTimeLocal->setLabel('Appointment Date');
$dateTimeLocal->setAttributes([
    'min'  => '2010-01-01T00:00:00',
    'max'  => '2020-01-01T00:00:00',
    'step' => '1', // minutes; default step interval is 1 min
]);
$dateTimeLocal->setOptions([
    'format' => 'Y-m-d\TH:i',
]);

$form = new Form('my-form');
$form->add($dateTimeLocal);
```

Here is with the array notation:

```php
use Zend\Form\Element;
use Zend\Form\Form;

$form = new Form('my-form');
$form->add([
	'type' => Element\DateTimeLocal::class,
	'name' => 'appointment-date-time',
	'options' => [
		'label'  => 'Appointment Date',
		'format' => 'Y-m-d\TH:i'
	],
	'attributes' => [
		'min' => '2010-01-01T00:00:00',
		'max' => '2020-01-01T00:00:00',
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

The following methods are specific to the `DateTimeLocal` element; all other methods
inherited from the [parent `DateTime` class](date-time.md#public-methods) are also
available.

Method signature                  | Description
--------------------------------- | -----------
`getInputSpecification() : array` | Returns an input filter specification, which includes `Zend\Filter\StringTrim` and will add the appropriate validators based on the values from the `min`, `max`, and `step` attributes and `format` option. See the [DateTime::getInputSpecification()](date-time.md#public-methods) definition for more information.
