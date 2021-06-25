# Range

`Zend\Form\Element\Range` is meant to be paired with the
[FormRange](../helper/form-range.md) helper for
[HTML5 inputs with type "range"](http://www.whatwg.org/specs/web-apps/current-work/multipage/states-of-the-type-attribute.html#range-state-%28type=range%29).
This element adds filters and validators to its input filter specification in
order to validate HTML5 range values on the server.

## Basic Usage

This element automatically adds a `type` attribute of value `range`.

```php
use Zend\Form\Element;
use Zend\Form\Form;

$range = new Element\Range('range');
$range->setLabel('Minimum and Maximum Amount');
$range->setAttributes([
    'min'  => '0',   // default minimum is 0
    'max'  => '100', // default maximum is 100
    'step' => '1',   // default interval is 1
]);

$form = new Form('my-form');
$form->add($range);
```

Using array notation:

```php
use Zend\Form\Element;
use Zend\Form\Form;

$form = new Form('my-form');
$form->add([
	'type' => Element\Range::class,
	'name' => 'range',
	'options' => [
		'label' => 'Minimum and Maximum Amount',
	],
	'attributes' => [
		'min' => 0, // default minimum is 0
		'max' => 100, // default maximum is 100
		'step' => 1, // default interval is 1
	],
]);
```

> ### Set all attributes before calling prepare
>
> The `min`, `max`, and `step` attributes should be set prior to calling
> `Zend\Form::prepare()`. Otherwise, the default input specification for the
> element may not contain the correct validation rules.

## Public Methods

The `Range` element extends the `Number` element, and inherits
[its methods](number.md#public-methods), with the following changes:

Method signature                  | Description
--------------------------------- | -----------
`getInputSpecification() : array` | Returns a input filter specification, which includes `Zend\Filter\StringTrim` and will add the appropriate validators based on the values from the `min`, `max`, and `step` attributes. See [the Number element](number.md#public-methods) for more information.  The `Range` element differs from `Zend\Form\Element\Number` in that the `Zend\Validator\GreaterThan` and `Zend\Validator\LessThan` validators will always be present. The default minimum is 1, and the default maximum is 100.
