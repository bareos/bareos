# Submit

`Zend\Form\Element\Submit` represents a submit button form input.
It should be used with the [FormSubmit](../helper/form-submit.md) view helper.

## Basic Usage

This element automatically adds a `type` attribute of value `submit`.

```php
use Zend\Form\Element;
use Zend\Form\Form;

$submit = new Element\Submit('my-submit');
$submit->setValue('Submit Form');

$form = new Form('my-form');
$form->add($submit);
```
