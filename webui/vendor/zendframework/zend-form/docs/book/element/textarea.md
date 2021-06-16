# Textarea

`Zend\Form\Element\Textarea` represents a textarea form input.
It should be used with the [FormTextarea](../helper/form-textarea.md) view helper.

## Basic Usage

This element automatically adds a `type` attribute of value `textarea`.

```php
use Zend\Form\Element;
use Zend\Form\Form;

$textarea = new Element\Textarea('my-textarea');
$textarea->setLabel('Enter a description');

$form = new Form('my-form');
$form->add($textarea);
```
