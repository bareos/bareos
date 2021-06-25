# Image

`Zend\Form\Element\Image` represents a image button form input.
It can be used with the [FormImage](../helper/form-image.md) view helper.

## Basic Usage

This element automatically adds a `type` attribute of value `image`.

```php
use Zend\Form\Element;
use Zend\Form\Form;

$image = new Element\Image('my-image');
$image->setAttribute('src', 'http://my.image.url'); // Src attribute is required

$form = new Form('my-form');
$form->add($image);
```
