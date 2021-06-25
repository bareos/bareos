# Button

`Zend\Form\Element\Button` represents a button form input.
It can be used with the `Zend\Form\View\Helper\FormButton` view helper.

`Zend\Form\Element\Button` extends from [`Zend\Form\Element`](element.md).

## Basic Usage

This element automatically adds a `type` attribute of value `button`.

```php
use Zend\Form\Element;
use Zend\Form\Form;

$button = new Element\Button('my-button');
$button->setLabel('My Button');
$button->setValue('foo');

$form = new Form('my-form');
$form->add($button);
```
