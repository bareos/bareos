# FormElement

The `FormElement` view helper proxies the rendering to specific form view
helpers depending on the type of the `Zend\Form\Element` provided. For instance,
if the provided element had a type of "text", the `FormElement` helper will
retrieve and use the `FormText` helper to render the element.

## Basic usage:

```php
use Zend\Form\Form;
use Zend\Form\Element;

// Within your view...

/**
 * Example #1: Render different types of form elements
 */
$textElement     = new Element\Text('my-text');
$checkboxElement = new Element\Checkbox('my-checkbox');

echo $this->formElement($textElement);
// Result: <input type="text" name="my-text" value="">

echo $this->formElement($checkboxElement);
// Result:
// <input type="hidden" name="my-checkbox" value="0">
// <input type="checkbox" name="my-checkbox" value="1">

/**
 * Example #2: Loop through form elements and render them
 */
$form = new Form();
// ...add elements and input filter to form...
$form->prepare();

// Render the opening tag
echo $this->form()->openTag($form);

// ...loop through and render the form elements...
foreach ($form as $element) {
    echo $this->formElement($element);
    echo $this->formElementErrors($element);
}

// Render the closing tag
echo $this->form()->closeTag();
```
