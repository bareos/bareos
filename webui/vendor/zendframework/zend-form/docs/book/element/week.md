# Week

`Zend\Form\Element\Week` is meant to be paired with the
[FormWeek](../helper/form-week.md) helper for
[HTML5 inputs with type "week"](http://www.whatwg.org/specs/web-apps/current-work/multipage/states-of-the-type-attribute.html#week-state-%28type=week%29).
This element adds filters and validators to its input filter specification in
order to validate HTML5 week input values on the server.

## Basic Usage

This element automatically adds a `type` attribute of value `week`.

```php
use Zend\Form\Element;
use Zend\Form\Form;

$week = new Element\Week('week');
$week->setLabel('Week');
$week->setAttributes([
    'min'  => '2012-W01',
    'max'  => '2020-W01',
    'step' => '1', // weeks; default step interval is 1 week
]);

$form = new Form('my-form');
$form->add($week);
```

Using array notation:

```php
use Zend\Form\Element;
use Zend\Form\Form;

$form = new Form('my-form');
$form->add([
	'type' => Element\Week::class,
	'name' => 'week',
	'options' => [
		'label' => 'Week',
	],
	'attributes' => [
		'min' => '2012-W01',
		'max' => '2020-W01',
		'step' => '1', // weeks; default step interval is 1 week
	],
]);
```

> ### Set all attributes before calling prepare
>
> The `min`, `max`, and `step` attributes should be set prior to calling
> `Zend\Form::prepare()`. Otherwise, the default input specification for the
> element may not contain the correct validation rules.

## Public Methods

The following methods are specific to the `Week` element; all other methods
inherited from the [parent `DateTime` class](date-time.md#public-methods) are also
available.

Method signature                  | Description
--------------------------------- | -----------
`getInputSpecification() : array` | Returns a input filter specification, which includes `Zend\Filter\StringTrim` and will add the appropriate validators based on the values from the `min`, `max`, and `step` attributes and `format` option.  See the [DateTime element](date-time.md#public-methods) for more information.  One difference from `Zend\Form\Element\DateTime` is that the `Zend\Validator\DateStep` validator will expect the `step` attribute to use an interval of weeks (default is 1 week).
