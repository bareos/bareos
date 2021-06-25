# FormSelect

The `FormSelect` view helper can be used to render an `<input type="select">`
HTML form input with its options. It is meant to work with the [Select element](../element/select.md),
which provides a default input specification for validating a select.

## Basic usage

```php
use Zend\Form\Element;

$element = new Element\Select('language');
$element->setValueOptions([
   '0' => 'French',
   '1' => 'English',
   '2' => 'Japanese',
   '3' => 'Chinese'
]);

// Within your view...
echo $this->formSelect($element);
```
