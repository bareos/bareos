# Email

`Zend\Form\Element\Email` is meant to be paired with the
[FormEmail](../helper/form-email.md) helper for
[HTML5 inputs with type "email"](http://www.whatwg.org/specs/web-apps/current-work/multipage/states-of-the-type-attribute.html#e-mail-state-%28type=email%29).
This element adds filters and validators to its input filter specification in
order to validate [HTML5 valid email address](http://www.whatwg.org/specs/web-apps/current-work/multipage/states-of-the-type-attribute.html#valid-e-mail-address)
on the server.

## Basic Usage

This element automatically adds a `type` attribute of value `email`.

```php
use Zend\Form\Element;
use Zend\Form\Form;

$form = new Form('my-form');

// Single email address:
$email = new Element\Email('email');
$email->setLabel('Email Address')
$form->add($email);

// Comma separated list of emails:
$emails = new Element\Email('emails');
$emails->setLabel('Email Addresses');
$emails->setAttribute('multiple', true);
$form->add($emails);
```

Using array notation:

```php
use Zend\Form\Element;
use Zend\Form\Form;

$form = new Form('my-form');

// Single email address:
$form->add([
	'type' => Element\Email::class,
	'name' => 'email',
	'options' => [
        'label' => 'Email Address',
	],
]);

// Comma separated list of emails:
$form->add([
	'type' => Element\Email::class,
	'name' => 'emails',
	'options' => [
		'label' => 'Email Addresses',
	],
	'attributes' => [
        'multiple' => true,
	],
]);
```

> ### Set multiple attribute before calling prepare
>
> Note: the `multiple` attribute should be set prior to calling
> `Zend\Form::prepare()`. Otherwise, the default input specification for the
> element may not contain the correct validation rules.

## Public Methods

The following methods are specific to the `Email` element; all other methods
defined by the [parent `Element` class](element.md#public-methods) are also
available.

Method signature                                          | Description
--------------------------------------------------------- | -----------
`getInputSpecification() : array`                         | See description below.
`setValidator(ValidatorInterface $validator) : void`      | Sets the primary validator to use for this element.
`getValidator() : ValidatorInterface`                     | Get the primary validator.
`setEmailValidator(ValidatorInterface $validator) : void` | Sets the email validator to use for multiple or single email addresses.
`getEmailValidator() : void`                              | Get the email validator to use for multiple or single email addresses. The default `Regex` validator in use is to match that of the browser validation, but you are free to set a different (more strict) email validator such as `Zend\Validator\Email` if you wish.

`getInputSpecification()` returns an input filter specification, which includes
a `Zend\Filter\StringTrim` filter, and a validator based on the `multiple`
attribute:

- If the `multiple` attribute is unset or `false`, a `Zend\Validator\Regex`
  validator will be added to validate a single email address.
- If the `multiple` attribute is `true`, a `Zend\Validator\Explode` validator
  will be added to ensure the input string value is split by commas before
  validating each email address with `Zend\Validator\Regex`.
