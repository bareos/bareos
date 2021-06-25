# Csrf

`Zend\Form\Element\Csrf` pairs with the [FormHidden](../helper/form-hidden.md)
helper to provide protection from CSRF attacks on forms, ensuring the data is
submitted by the user session that generated the form and not by a rogue script.
Protection is achieved by adding a hash element to a form and verifying it when
the form is submitted.

## Basic Usage

This element automatically adds a `type` attribute of value `hidden`.

```php
use Zend\Form\Element;
use Zend\Form\Form;

$csrf = new Element\Csrf('csrf');

$form = new Form('my-form');
$form->add($csrf);
```

You can change the options of the CSRF validator using the
`setCsrfValidatorOptions()` function, or by using the `csrf_options` key. Here
is an example using array notation:

```php
use Zend\Form\Element;
use Zend\Form\Form;

$form = new Form('my-form');
$form->add([
	'type' => Element\Csrf::class,
	'name' => 'csrf',
	'options' => [
		'csrf_options' => [
			'timeout' => 600,
		],
	],
]);
```

> ### Multiple CSRF elements must be uniquely named
>
> If you are using more than one form on a page, and each contains its own CSRF
> element, you will need to make sure that each form uniquely names its element;
> if you do not, it's possible for the value of one to override the other within
> the server-side session storage, leading to the inability to validate one or
> more of the forms on your page. We suggest prefixing the element name with the
> form's name or function: `login_csrf`, `registration_csrf`, etc.

## Public Methods

The following methods are specific to the `Csrf` element; all other methods
defined by the [parent `Element` class](element.md#public-methods) are also
available.

Method signature                                          | Description
--------------------------------------------------------- | -----------
`getInputSpecification() : array`                         | Returns an input filter specification, which includes a `Zend\Filter\StringTrim` filter and `Zend\Validator\Csrf` to validate the CSRF value.
`setCsrfValidatorOptions(array $options) : void`          | Set the options that are used by the CSRF validator.
`getCsrfValidatorOptions() : array`                       | Get the options that are used by the CSRF validator.
`setCsrfValidator(Zend\Validator\Csrf $validator) : void` | Override the default CSRF validator by setting another one.
`getCsrfValidator() : Zend\Validator\Csrf `               | Get the CSRF validator.
