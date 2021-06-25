# FormLabel

The `FormLabel` view helper is used to render a `<label>` HTML element and its
attributes. If you have a `Zend\I18n\Translator\Translator` attached,
`FormLabel` will translate the label contents when rendering.

## Basic usage

```php
use Zend\Form\Element;

$element = new Element\Text('my-text');
$element->setLabel('Label');
$element->setAttribute('id', 'text-id');
$element->setLabelAttributes(['class' => 'control-label']);

// Within your view...

/**
 * Example #1: Render label in one shot
 */
echo $this->formLabel($element);
// Result: <label class="control-label" for="text-id">Label</label>

echo $this->formLabel($element, $this->formText($element));
// Result: <label class="control-label" for="text-id">Label<input type="text" name="my-text"></label>

echo $this->formLabel($element, $this->formText($element), 'append');
// Result: <label class="control-label" for="text-id"><input type="text" name="my-text">Label</label>

/**
 * Example #2: Render label in separate steps
 */
// Render the opening tag
echo $this->formLabel()->openTag($element);
// Result: <label class="control-label" for="text-id">

// Render the closing tag
echo $this->formLabel()->closeTag();
// Result: </label>

/**
 * Example #3: Render html label after toggling off escape
 */
$element->setLabel('<abbr title="Completely Automated Public Turing test to tell Computers and Humans Apart">CAPTCHA</abbr>');
$element->setLabelOptions(['disable_html_escape' => true]);
echo $this->formLabel($element);
// Result:
// <label class="control-label" for="text-id">
//     <abbr title="Completely Automated Public Turing test to tell Computers and Humans Apart">CAPTCHA</abbr>
// </label>
```

> ### Escaping
>
> HTML escaping only applies to the `Element::$label` property, not to the
> helper `$labelContent` parameter.

## Label translation

See [AbstractHelper Translation](abstract-helper.md#translation).

## Public methods

The following public methods are in addition to those inherited from the
[AbstractHelper](abstract-helper.md#public-methods):

Method signature                                                       | Description
---------------------------------------------------------------------- | -----------
`__invoke(ElementInterface $element = null, string $labelContent = null, string $position = null) : string` | Render a form label, optionally with content.  Always generates a `for` attribute, as we cannot assume the form input will be provided in the `$labelContent`. If the `$labelContent` is `null`, the helper uses the element's label value. `$position` is used to indicate where the label element should be rendered, and should be one of `FormLabel::APPEND` or `FormLabel::PREPEND` (the default).
`openTag(array|ElementInterface $attributesOrElement = null) : string` | Renders the `<label>` open tag and attributes. `$attributesOrElement` should be an array of key/value pairs representing label attributes, or an `ElementInterface` instance.
`closeTag() : string`                                                  | Renders a `</label>` closing tag.
