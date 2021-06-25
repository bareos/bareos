# FormButton

The `FormButton` view helper is used to render a `<button>` HTML element and its
attributes.

## Basic usage

```php
use Zend\Form\Element;

$element = new Element\Button('my-button');
$element->setLabel('Reset');

// Within your view...

/**
 * Example #1: Render entire button in one shot...
 */
echo $this->formButton($element);
// Result: <button name="my-button" type="button">Reset</button>

/**
 * Example #2: Render button in 3 steps
 */
// Render the opening tag
echo $this->formButton()->openTag($element);
// Result: <button name="my-button" type="button">

echo '<span class="inner">' . $element->getLabel() . '</span>';

// Render the closing tag
echo $this->formButton()->closeTag();
// Result: </button>

/**
 * Example #3: Override the element label
 */
echo $this->formButton()->render($element, 'My Content');
// Result: <button name="my-button" type="button">My Content</button>
```

## Public methods

The following public methods are in addition to those inherited from the
[FormInput](form-input.md#public-methods):

Method signature                                                       | Description
---------------------------------------------------------------------- | -----------
`openTag($element = null) : string`                                    | Renders the `<button>` open tag for the `$element` instance.
`closeTag() : string`                                                  | Renders a `</button>` closing tag.
`render(ElementInterface $element [, $buttonContent = null]) : string` | Renders a button's opening tag, inner content, and closing tag. If `$buttonContent` is `null`, defaults to `$element`'s label.
