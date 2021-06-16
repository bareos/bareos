# Url

`Zend\Form\Element\Url` is meant to be paired with the
[FormUrl](../helper/form-url.md) helper for
[HTML5 inputs with type "url"](http://www.whatwg.org/specs/web-apps/current-work/multipage/states-of-the-type-attribute.html#url-state-%28type=url%29).
This element adds filters and a `Zend\Validator\Uri` validator to its input
filter specification for validating HTML5 URL input values on the server.

## Basic Usage

This element automatically adds a `type` attribute of value `url`.

```php
use Zend\Form\Element;
use Zend\Form\Form;

$url = new Element\Url('webpage-url');
$url->setLabel('Webpage URL');

$form = new Form('my-form');
$form->add($url);
```

Using array notation:

```php
use Zend\Form\Element;
use Zend\Form\Form;

$form = new Form('my-form');
$form->add([
	'type' => Element\Url::class,
	'name' => 'webpage-url',
	'options' => [
		'label' => 'Webpage URL',
	],
]);
```

## Public Methods

The following methods are specific to the `Url` element; all other methods
defined by the [parent `Element` class](element.md#public-methods) are also
available.

Method signature                  | Description
--------------------------------- | -----------
`getInputSpecification() : array` | Returns a input filter specification, which includes a `Zend\Filter\StringTrim` filter, and a `Zend\Validator\Uri` to validate the URI string.
