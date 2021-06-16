# FormMultiCheckbox

The `FormMultiCheckbox` view helper can be used to render a group of `<input
type="checkbox">` HTML form inputs. It is meant to work with the
[MultiCheckbox element](../element/multi-checkbox.md), which provides a default
input specification for validating checkbox groups.

## Basic usage

```php
use Zend\Form\Element;

$element = new Element\MultiCheckbox('my-multicheckbox');
$element->setValueOptions([
   '0' => 'Apple',
   '1' => 'Orange',
   '2' => 'Lemon',
]);

// Within your view...

/**
 * Example #1: using the default label placement
 */
echo $this->formMultiCheckbox($element);
// Result:
// <label><input type="checkbox" name="my-multicheckbox[]" value="0">Apple</label>
// <label><input type="checkbox" name="my-multicheckbox[]" value="1">Orange</label>
// <label><input type="checkbox" name="my-multicheckbox[]" value="2">Lemon</label>

/**
 * Example #2: using the prepend label placement
 */
echo $this->formMultiCheckbox($element, 'prepend');
// Result:
// <label>Apple<input type="checkbox" name="my-multicheckbox[]" value="0"></label>
// <label>Orange<input type="checkbox" name="my-multicheckbox[]" value="1"></label>
// <label>Lemon<input type="checkbox" name="my-multicheckbox[]" value="2"></label>
```
