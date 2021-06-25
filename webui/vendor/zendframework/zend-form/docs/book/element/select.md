# Select

`Zend\Form\Element\Select` is meant to be paired with
[FormSelect](../helper/form-select.md) for HTML inputs with type "select". This
element adds an `InArray` validator to its input filter specification in order
to validate on the server if the selected value belongs to the values. This
element can be used as a multi-select element by adding the `multiple` HTML
attribute to the element.

## Basic Usage

This element automatically adds a `type` attribute of value `select`.

```php
use Zend\Form\Element;
use Zend\Form\Form;

$select = new Element\Select('language');
$select->setLabel('Which is your mother tongue?');
$select->setValueOptions([
	'0' => 'French',
	'1' => 'English',
	'2' => 'Japanese',
	'3' => 'Chinese',
]);

$form = new Form('language');
$form->add($select);
```

Using array notation:

```php
use Zend\Form\Element;
use Zend\Form\Form;

$form = new Form('my-form');
$form->add([
	'type' => Element\Select::class,
	'name' => 'language',
	'options' => [
		'label' => 'Which is your mother tongue?',
		'value_options' => [
			'0' => 'French',
			'1' => 'English',
			'2' => 'Japanese',
			'3' => 'Chinese',
		],
	],
]);
```

You can add an empty option (option with no value) using the `empty_option`
option:

```php
use Zend\Form\Element;
use Zend\Form\Form;

$form = new Form('my-form');
$form->add([
	'type' => Element\Select::class,
	'name' => 'language',
	'options' => [
		'label' => 'Which is your mother tongue?',
        'empty_option' => 'Please choose your language',
		'value_options' => [
			'0' => 'French',
			'1' => 'English',
			'2' => 'Japanese',
			'3' => 'Chinese',
		],
	],
]);
```

Option groups are also supported by adding an `options` key to the value options:

```php
use Zend\Form\Element;
use Zend\Form\Form;

$select = new Element\Select('language');
$select->setLabel('Which is your mother tongue?');
$select->setValueOptions([
     'european' => [
        'label' => 'European languages',
        'options' => [
           '0' => 'French',
           '1' => 'Italian',
        ],
     ],
     'asian' => [
        'label' => 'Asian languages',
        'options' => [
           '2' => 'Japanese',
           '3' => 'Chinese',
        ],
     ],
]);

$form = new Form('language');
$form->add($select);
```

Attributes on the options are supported by providing the options as a
specification array instead of key-value pairs:

```php
$select->setValueOptions([
    [
        'value' => '0',
        'label' => 'French',
        'attributes' => [
            'data-locale' = 'fr'
        ],
    ],
    [
        'value' => '1',
        'label' => 'Italian',
        'disabled' => true,
    ],
]);
```

## Public Methods

The following methods are specific to the `Select` element; all other methods
defined by the [parent `Element` class](element.md#public-methods) are also
available.

Method signature                         | Description
---------------------------------------- | -----------
`setOptions(array $options) : void`      | Set options for an element. Accepted options, in addition to the inherited options, include `value_options`, `empty_option`, and `disable_inarray_validator`, which call `setValueOptions()`, `setEmptyOption()` and `setDisableInArrayValidator()`, respectively.
`setValueOptions(array $options) : void` | Set the value options for the select element. The array must contain key/value pairs.
`getValueOptions() : array`              | Return the value options.
`unsetValueOption($key) : void`          | Unset the value option from the select element.
`setEmptyOption($emptyOption) : void`    | Optionally set a label for an empty option (option with no value). It is set to `null` by default, which means that no empty option will be rendered.
`getEmptyOption() : string|null`         | Get the label for the empty option (`null` if none).
