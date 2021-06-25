# FormCheckbox

The `FormCheckbox` view helper can be used to render an `<input
type="checkbox">` HTML form input. It is meant to work with the
[checkbox element](../element/checkbox.md), which provides a default input
specification for validating the checkbox values.

## Basic usage

```php
use Zend\Form\Element;

$element = new Element\Checkbox('my-checkbox');

// Within your view...

/**
 * Example #1: Default options
 */
echo $this->formCheckbox($element);
// Result:
// <input type="hidden" name="my-checkbox" value="0">
// <input type="checkbox" name="my-checkbox" value="1">

/**
 * Example #2: Disable hidden element
 */
$element->setUseHiddenElement(false);
echo $this->formCheckbox($element);
// Result: <input type="checkbox" name="my-checkbox" value="1">

/**
 * Example #3: Change checked/unchecked values
 */
$element->setUseHiddenElement(true);
$element->setUncheckedValue('no');
$element->setCheckedValue('yes');
echo $this->formCheckbox($element);
// Result:
// <input type="hidden" name="my-checkbox" value="no">
// <input type="checkbox" name="my-checkbox" value="yes">
```
