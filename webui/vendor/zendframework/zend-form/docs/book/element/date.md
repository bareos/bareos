# Date

`Zend\Form\Element\Date` is meant to be paired with the
[FormDate](../helper/form-date.md) helper for
[HTML5 inputs with type "date"](http://www.whatwg.org/specs/web-apps/current-work/multipage/states-of-the-type-attribute.html#date-state-%28type=date%29).
This element adds filters and validators to its input filter specification in
order to validate HTML5 date input values on the server.

## Basic Usage

This element automatically adds a `type` attribute of value `date`.

```php
use Zend\Form\Element;
use Zend\Form\Form;

$date = new Element\Date('appointment-date');
$date->setLabel('Appointment Date');
$date->setAttributes([
    'min'  => '2012-01-01',
    'max'  => '2020-01-01',
    'step' => '1', // days; default step interval is 1 day
]);
$date->setOptions([
    'format' => 'Y-m-d',
]);

$form = new Form('my-form');
$form->add($date);
```

The following uses array notation:

```php
use Zend\Form\Element;
use Zend\Form\Form;

$form = new Form('my-form');
$form->add([
	'type' => Element\Date::class,
	'name' => 'appointment-date',
	'options' => [
		'label' => 'Appointment Date',
		'format' => 'Y-m-d',
	],
	'attributes' => [
		'min' => '2012-01-01',
		'max' => '2020-01-01',
		'step' => '1', // days; default step interval is 1 day
	],
]);
```

> ### Set all attributes before calling prepare
>
> The `min`, `max`, and `step` attributes should be set prior to calling
> `Zend\Form::prepare()`. Otherwise, the default input specification for the
> element may not contain the correct validation rules.

## Public Methods

The following methods are specific to the `Date` element; all other methods
inherited from the [parent `DateTime` class](date-time.md#public-methods) are also
available.

Method signature                  | Description
--------------------------------- | -----------
`getInputSpecification() : array` | Returns an input filter specification, which includes `Zend\Filter\StringTrim` and will add the appropriate validators based on the values from the `min`, `max`, and `step` attributes and `format` option. See the [DateTime::getInputSpecification()](date-time.md#public-methods) definition for more information. One difference from `Zend\Form\Element\DateTime` is that the `Zend\Validator\DateStep` validator will expect the `step` attribute to use an interval of days (default is 1 day).
