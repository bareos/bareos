# Hidden

`Zend\Form\Element\Hidden` represents a hidden form input.
It can be used with the [FormHidden](../helper/form-hidden.md) view helper.

## Basic Usage

This element automatically adds a `type` attribute of value `hidden`.

```php
use Zend\Form\Element;
use Zend\Form\Form;

$hidden = new Element\Hidden('my-hidden');
$hidden->setValue('foo');

$form = new Form('my-form');
$form->add($hidden);
```

Using array notation:

```php
use Zend\Form\Element;
use Zend\Form\Form;

$form = new Form('my-form');
$form->add([
	'type' => Element\Hidden::class,
	'name' => 'my-hidden',
	'attributes' => [
		'value' => 'foo',
	],
]);
```
