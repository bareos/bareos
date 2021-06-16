# Color

`Zend\Form\Element\Color` is meant to be paired with
`Zend\Form\View\Helper\FormColor` for [HTML5 inputs with type "color"](http://www.whatwg.org/specs/web-apps/current-work/multipage/states-of-the-type-attribute.html#color-state-%28type=color%29).
This element adds filters and a `Regex` validator to its input filter
specification in order to validate an [HTML5 valid simple color](http://www.whatwg.org/specs/web-apps/current-work/multipage/common-microsyntaxes.html#valid-simple-color)
value on the server.

## Basic Usage

This element automatically adds a `type` attribute of value `color`.

```php
use Zend\Form\Element;
use Zend\Form\Form;

$color = new Element\Color('color');
$color->setLabel('Background color');

$form = new Form('my-form');
$form->add($color);
```

The following uses array notation instead:

```php
use Zend\Form\Element;
use Zend\Form\Form;

$form = new Form('my-form');
$form->add([
	'type' => Element\Color::class,
	'name' => 'color',
	'options' => [
		'label' => 'Background color',
	],
]);
```

## Public Methods

The following methods are specific to the `Color` element; all other methods
defined by the [parent `Element` class](element.md#public-methods) are also
available.

Method signature                  | Description
--------------------------------- | -----------
`getInputSpecification() : array` | Returns an input filter specification, which includes `Zend\Filter\StringTrim` and `Zend\Filter\StringToLower` filters, and a `Zend\Validator\Regex` to validate the RGB hex format.
