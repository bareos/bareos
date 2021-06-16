# Text

`Zend\Form\Element\Text` represents a text form input.
It should be used with the [FormText](../helper/form-text.md) view helper.

## Basic Usage

This element automatically adds a `type` attribute of value `text`.

```php
use Zend\Form\Element;
use Zend\Form\Form;

$text = new Element\Text('my-text');
$text->setLabel('Enter your name');

$form = new Form('my-form');
$form->add($text);
```
