# FormRow

The `FormRow` view helper is used by `Form` view helper to render each row of a
form, but can also be used stand-alone. A form row usually consists of the
output produced by the helper specific to an input, plus its label and errors,
if any.

`FormRow` handles different rendering options, having elements wrapped by the
`<label\>` HTML block by default, but also allowing to render them in separate
blocks when the element has an `id` attribute specified, thus preserving browser
usability features.

Other options involve label positioning, escaping, toggling errors and using
custom partial templates.

## Basic Usage

```php
/**
 * inside view template
 *
 * @var \Zend\View\Renderer\PhpRenderer $this
 * @var \Zend\Form\Form $form
 */

// Prepare the form
$form->prepare();

// Render the opening tag
echo $this->form()->openTag($form);

$element = $form->get('some_element');
$element->setLabel('Some Label');

// Render 'some_element' label, input, and errors if any:
echo $this->formRow($element);
// Result: <label><span>Some Label</span><input type="text" name="some_element" value=""></label>

// Altering label position:
echo $this->formRow($element, 'append');
// Result: <label><input type="text" name="some_element" value=""><span>Some Label</span></label>

// Setting the 'id' attribute will result in a separated label rather than a
// wrapping one:
$element->setAttribute('id', 'element_id');
echo $this->formRow($element);
// Result: <label for="element_id">Some Label</label><input type="text" name="some_element" id="element_id" value="">

// Turn off escaping for HTML labels:
$element->setLabel('<abbr title="Completely Automated Public Turing test to tell Computers and Humans Apart">CAPTCHA</abbr>');
$element->setLabelOptions(['disable_html_escape' => true]);
// Result:
// <label>
//   <span>
//       <abbr title="Completely Automated Public Turing test to tell Computers and Humans Apart">CAPTCHA</abbr>
//   </span>
//   <input type="text" name="some_element" value="">
// </label>

// Render the closing tag
echo $this->form()->closeTag();
```

> ### Escaping
>
> Label content is escaped by default.
