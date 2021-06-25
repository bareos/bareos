# MultiCheckbox

`Zend\Form\Element\MultiCheckbox` is meant to be paired with the
[FormMultiCheckbox](../helper/form-multicheckbox.md) for HTML inputs with type
"checkbox".  This element adds an `InArray` validator to its input filter
specification in order to validate on the server if the checkbox contains values
from the multiple checkboxes.

## Basic Usage

This element automatically adds a `type` attribute of value `checkbox` for every
checkboxes.

```php
use Zend\Form\Element;
use Zend\Form\Form;

$multiCheckbox = new Element\MultiCheckbox('multi-checkbox');
$multiCheckbox->setLabel('What do you like ?');
$multiCheckbox->setValueOptions([
        '0' => 'Apple',
        '1' => 'Orange',
        '2' => 'Lemon'
]);

$form = new Form('my-form');
$form->add($multiCheckbox);
```

Using array notation:

```php
use Zend\Form\Element;
use Zend\Form\Form;

$form = new Form('my-form');
$form->add([
    'type' => Element\MultiCheckbox::class,
    'name' => 'multi-checkbox',
    'options' => [
        'label' => 'What do you like ?',
        'value_options' => [
            '0' => 'Apple',
            '1' => 'Orange',
            '2' => 'Lemon',
        ],
    ],
]);
```

## Advanced Usage

In order to set attributes or customize the option elements, an array can be
used instead of a string. The following keys are supported:

- `label`: The string displayed for the option.
- `value`: The form value associated with the option.
- `selected`: Boolean that sets whether the option is marked as selected.
- `disabled`: Boolean that sets whether the option will be disabled
- `attributes`: Array of HTML attributes that will be set on this option. Merged
  with the attributes set on the element.
- `label_attributes`: Array of HTML attributes that will be set on the label.
  Merged with the attributes set on the element's label.

```php
$form = new Form('my-form');
$form->add([
    'type' => Element\MultiCheckbox::class,
    'name' => 'multi-checkbox',
    'options' => [
        'label' => 'What do you like ?',
        'value_options' => [
            [
                'value' => '0',
                'label' => 'Apple',
                'selected' => false,
                'disabled' => false,
                'attributes' => [
                    'id' => 'apple_option',
                    'data-fruit' => 'apple',
                ],
                'label_attributes' => [
                    'id' => 'apple_label',
                ],
            ],
            [
                'value' => '1',
                'label' => 'Orange',
                'selected' => true,
            ],
            [
                'value' => '2',
                'label' => 'Lemon',
            ],
        ],
    ],
]);
```

## Public Methods

The following methods are specific to the `MultiCheckbox` element; all other methods
defined by the [parent `Element` class](element.md#public-methods) are also
available.

Method signature                         | Description
---------------------------------------- | -----------
`setOptions(array $options) : void`      | Set options for an element of type Checkbox. In addition to the options inherited from [Checkbox](checkbox.md#public-methods) allows `value_options`, which calls `setValueOptions()`.
`setValueOptions(array $options) : void` | Set the value options for every checkbox of the multi-checkbox. The array must contain a key/value pair for every checkbox.
`getValueOptions() : array`              | Return the value options.
`unsetValueOption($key) : void`          | Unset the value option from the multi-checkbox.
