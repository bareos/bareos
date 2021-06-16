# Time

`Zend\Form\Element\Time` is meant to be paired with the
[FormTime](../helper/form-time.md) helper for
[HTML5 inputs with type "time"](http://www.whatwg.org/specs/web-apps/current-work/multipage/states-of-the-type-attribute.html#time-state-%28type=time%29).
This element adds filters and validators to its input filter specification in
order to validate HTML5 time input values on the server.

## Basic Usage

This element automatically adds a `type` attribute of value `time`.

```php
use Zend\Form\Element;
use Zend\Form\Form;

$time = new Element\Time('time');
$time->setLabel('Time');
$time->setAttributes([
    'min'  => '00:00:00',
    'max'  => '23:59:59',
    'step' => '60', // seconds; default step interval is 60 seconds
]);
$time->setOptions([
    'format' => 'H:i:s',
]);

$form = new Form('my-form');
$form->add($time);
```

Using array notation:

```php
use Zend\Form\Element;
use Zend\Form\Form;

$form = new Form('my-form');
$form->add([
	'type' => Element\Time::class,
	'name' => 'time',
	'options'=> [
		'label'  => 'Time',
		'format' => 'H:i:s',
	],
	'attributes' => [
		'min' => '00:00:00',
		'max' => '23:59:59',
		'step' => '60', // seconds; default step interval is 60 seconds
	],
]);
```

> ### Set all attributes before calling prepare
>
> The `min`, `max`, and `step` attributes should be set prior to calling
> `Zend\Form::prepare()`. Otherwise, the default input specification for the
> element may not contain the correct validation rules.

> ### Default date format
>
> The default date format for the validator is `H:i:s`. However, a valid time
> string is not required to have a "seconds" representation. In fact, some user
> agent UIs such as Google Chrome and Opera submit time elements using the `H:i`
> format (i.e.  without a seconds representation). Set the date format
> accordingly.

## Public Methods

The following methods are specific to the `Time` element; all other methods
inherited from the [parent `DateTime` class](date-time.md#public-methods) are also
available.

Method signature                  | Description
--------------------------------- | -----------
`getInputSpecification() : array` | Returns a input filter specification, which includes `Zend\Filter\StringTrim` and will add the appropriate validators based on the values from the `min`, `max`, and `step` attributes and `format` option. See the [DateTime element](date-time.md#public-methods) for more information.  One difference from `Zend\Form\Element\DateTime` is that the `Zend\Validator\DateStep` validator will expect the `step` attribute to use an interval of seconds (default is 60 seconds).
