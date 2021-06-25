# Number

`Zend\Form\Element\Number` is meant to be paired with the
[FormNumber](../helper/form-number.md) helper for
[HTML5 inputs with type "number"](http://www.whatwg.org/specs/web-apps/current-work/multipage/states-of-the-type-attribute.html#number-state-%28type=number%29).
This element adds filters and validators to its input filter specification in
order to validate HTML5 number input values on the server.

## Basic Usage

This element automatically adds a `type` attribute of value `number`.

```php
use Zend\Form\Element;
use Zend\Form\Form;

$number = new Element\Number('quantity');
$number->setLabel('Quantity');
$number->setAttributes([
    'min'  => '0',
    'max'  => '10',
    'step' => '1', // default step interval is 1
]);

$form = new Form('my-form');
$form->add($number);
```

Using array notation:

```php
use Zend\Form\Element;
use Zend\Form\Form;

$form = new Form('my-form');
$form->add([
	'type' => Element\Number::class,
	'name' => 'quantity',
	'options' => [
		'label' => 'Quantity',
	],
	'attributes' => [
		'min' => '0',
		'max' => '10',
		'step' => '1', // default step interval is 1
	],
]);
```

> ### Set all attributes before calling prepare
>
> The `min`, `max`, and `step` attributes should be set prior to calling
> `Zend\Form::prepare()`. Otherwise, the default input specification for the
> element may not contain the correct validation rules.

## Public Methods

The following methods are specific to the `Number` element; all other methods
defined by the [parent `Element` class](element.md#public-methods) are also
available.

Method signature                  | Description
--------------------------------- | -----------
`getInputSpecification() : array` | Returns a input filter specification, which includes `Zend\Filter\StringTrim` and will add the appropriate validators based on the values from the `min`, `max`, and `step` attributes. See below for specifics.

`getInputSpecification()` will behave as follows based on the attributes
provided:

- If the `min` attribute is set, a `Zend\Validator\GreaterThan` validator will
  be added to ensure the number value is greater than the minimum value. The
  `min` value should be a [valid floating point number](http://www.whatwg.org/specs/web-apps/current-work/multipage/common-microsyntaxes.html#valid-floating-point-number).
- If the `max` attribute is set, a `Zend\Validator\LessThan` validator will be
  added to ensure the number value is less than the maximum value. The `max`
  value should be a [valid floating point number](http://www.whatwg.org/specs/web-apps/current-work/multipage/common-microsyntaxes.html#valid-floating-point-number).
- If the `step` attribute is set to "any", step validations will be skipped.
  Otherwise, a `Zend\Validator\Step` validator will be added to ensure the
  number value is within a certain interval (default is 1). The `step` value
  should be either "any" or a [valid floating point number](http://www.whatwg.org/specs/web-apps/current-work/multipage/common-microsyntaxes.html#valid-floating-point-number).
