# Tel

`Zend\Form\Element\Tel` is meant to be paired with the
[FormTel](../helper/form-tel.md) helper for
[HTML5 inputs with type "tel"](http://www.whatwg.org/specs/web-apps/current-work/multipage/states-of-the-type-attribute.html#telephone-state-%28type=tel%29).
This element adds filters and validators to its input filter specification in
order to validate HTML5 tel input values on the server.

**Available since 2.9.0.**

## Basic Usage

This element automatically adds a `type` attribute of value `tel`.

```php
use Zend\Form\Element;
use Zend\Form\Form;

$phone = new Element\Tel('phone');
$phone->setLabel('Phone');

$form = new Form('my-form');
$form->add($phone);
```

Using array notation:

```php
use Zend\Form\Element;
use Zend\Form\Form;

$form = new Form('my-form');
$form->add([
	'type' => Element\Tel::class,
	'name' => 'phone',
	'options' => [
		'label' => 'Phone',
	],
]);
```

## Public Methods

The following methods are specific to the `Tel` element:

Method signature                  | Description
--------------------------------- | -----------
`getInputSpecification() : array` | Returns an input filter specification, which includes `Zend\Filter\StringTrim` and `Zend\Filter\StripNewlines`, as well as a regex validator to ensure the value does not contain any carriage returns or newlines within it.
