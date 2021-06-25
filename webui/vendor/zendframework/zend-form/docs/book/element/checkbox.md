# Checkbox

`Zend\Form\Element\Checkbox` is meant to be paired with the
`Zend\Form\View\Helper\FormCheckbox` for HTML inputs with type checkbox. This
element adds an `InArray` validator to its input filter specification in order
to validate on the server if the checkbox contains either the checked value or
the unchecked value.

## Basic Usage

This element automatically adds a `type` attribute of value `checkbox`.

```php
use Zend\Form\Element;
use Zend\Form\Form;

$checkbox = new Element\Checkbox('checkbox');
$checkbox->setLabel('A checkbox');
$checkbox->setUseHiddenElement(true);
$checkbox->setCheckedValue('good');
$checkbox->setUncheckedValue('bad');

$form = new Form('my-form');
$form->add($checkbox);
```

Using array notation:

```php
use Zend\Form\Element;
use Zend\Form\Form;

$form = new Form('my-form');
$form->add([
    'type' => Element\Checkbox::class,
    'name' => 'checkbox',
    'options' => [
        'label' => 'A checkbox',
        'use_hidden_element' => true,
        'checked_value' => 'good',
        'unchecked_value' => 'bad',
    ],
]);
```

When creating a checkbox element, setting an attribute of `checked` will result
in the checkbox always being checked, regardless of any data object which might
subsequently be bound to the form. The correct way to set the default value of a
checkbox is to set the `value` attribute as for any other element. To have a
checkbox checked by default, make the `value` equal to the `checked_value` eg:

```php
use Zend\Form\Element;
use Zend\Form\Form;

$form = new Form('my-form');
$form->add([
    'type' => Element\Checkbox::class,
    'name' => 'checkbox',
    'options' => [
        'label' => 'A checkbox',
        'use_hidden_element' => true,
        'checked_value' => 'yes',
        'unchecked_value' => 'no',
    ],
    'attributes' => [
         'value' => 'yes',
    ],
]);
```

## Public Methods

The following methods are specific to the `Checkbox` element; all other methods
defined by the [parent `Element` class](element.md#public-methods) are also
available.

Method signature                                        | Description
------------------------------------------------------- | -----------
`setOptions(array $options) : void`                     | Set options for an element of type Checkbox. Accepted options, in addition to the options inherited from [Element](element.md#public-methods) , are: `use_hidden_element`, `checked_value` and `unchecked_value` , which call `setUseHiddenElement()`, `setCheckedValue()` and `setUncheckedValue()` , respectively.
`setUseHiddenElement(boolean $useHiddenElement) : void` | If set to true (which is default), the view helper will generate a hidden element that contains the unchecked value. Therefore, when using custom unchecked value, this option have to be set to true.
`useHiddenElement() : boolean`                          | Return if a hidden element is generated.
`setCheckedValue(string $checkedValue) : void`          | Set the value to use when the checkbox is checked.
`getCheckedValue() : string`                            | Return the value used when the checkbox is checked.
`setUncheckedValue(string $uncheckedValue) : void`      | Set the value to use when the checkbox is unchecked. For this to work, you must make sure that `use_hidden_element` is set to true.
`getUncheckedValue() : string`                          | Return the value used when the checkbox is unchecked.
`getInputSpecification() : array`                       | Returns an input filter specification, which includes `Zend\Validator\InArray` to validate if the value is one of the `checked` or `unchecked` values.
`isChecked() : boolean`                                 | Checks if the checkbox is checked.
`setChecked(bool $value) : void`                        | Checks or unchecks the checkbox.
