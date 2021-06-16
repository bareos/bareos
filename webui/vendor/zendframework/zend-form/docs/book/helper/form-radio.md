# FormRadio

The `FormRadio` view helper can be used to render a group of `<input
type="radio">` HTML form inputs. It is meant to work with the
[Radio element](../element/radio.md), which provides a default input
specification for validating a radio.

`FormRadio` extends from [FormMultiCheckbox](form-multicheckbox.md).

## Basic usage

```php
use Zend\Form\Element;

$element = new Element\Radio('likes_chocolate');
$element->setValueOptions([
   '0' => 'No',
   '1' => 'Yes',
]);

// Within your view...

/**
 * Example #1: using the default label placement
 */
echo $this->formRadio($element);
// Result:
// <label><input type="radio" name="likes_chocolate[]" value="0">No</label>
// <label><input type="radio" name="likes_chocolate[]" value="1">Yes</label>

/**
 * Example #2: using the prepend label placement
 */
echo $this->formRadio($element, 'prepend');
// Result:
// <label>No<input type="radio" name="likes_chocolate[]" value="0"></label>
// <label>Yes<input type="radio" name="likes_chocolate[]" value="1"></label>
```
