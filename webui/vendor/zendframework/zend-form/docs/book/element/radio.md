# Radio

`Zend\Form\Element\Radio` is meant to be paired with the
[FormRadio](../helper/form-radio.md) helper for HTML inputs with type "radio".
This element adds an `InArray` validator to its input filter specification in
order to validate on the server if the value is contains within the radio value
elements.

## Basic Usage

This element automatically adds a `type` attribute of value `radio` for every
radio.

```php
use Zend\Form\Element;
use Zend\Form\Form;

$radio = new Element\Radio('likes_chocolate');
$radio->setLabel('Do you like chocolate?');
$radio->setValueOptions([
    '0' => 'No',
    '1' => 'Yes',
]);

$form = new Form('my-form');
$form->add($radio);
```

Using array notation:

```php
use Zend\Form\Element;
use Zend\Form\Form;

$form = new Form('my-form');
$form->add([
    'type' => Element\Radio::class,
    'name' => 'likes_chocolate',
    'options' => [
        'label' => 'Do you like chocolate?',
        'value_options' => [
            '0' => 'No',
            '1' => 'Yes',
        ],
    ],
]);
```

## Advanced Usage

See [MultiCheckbox for examples](multi-checkbox.md) of how to apply attributes
and options to each radio button.

## Public Methods

The `Radio` element extends the `MultiCheckbox` element, and inherits
[its methods](multi-checkbox.md#public-methods).
