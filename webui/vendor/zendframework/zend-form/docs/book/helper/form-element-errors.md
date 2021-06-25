# FormElementErrors

The `FormElementErrors` view helper is used to render the validation error
messages of an element.

## Basic usage

```php
use Zend\Form\Form;
use Zend\Form\Element;
use Zend\InputFilter\InputFilter;
use Zend\InputFilter\Input;

// Create a form
$form    = new Form();
$element = new Element\Text('my-text');
$form->add($element);

// Create a input
$input = new Input('my-text');
$input->setRequired(true);

$inputFilter = new InputFilter();
$inputFilter->add($input);
$form->setInputFilter($inputFilter);

// Force a failure
$form->setData(array()); // Empty data
$form->isValid();        // Not valid

// Within your view...

/**
 * Example #1: Default options
 */
echo $this->formElementErrors($element);
// Result: <ul><li>Value is required and can&#039;t be empty</li></ul>

/**
 * Example #2: Add attributes to open format
 */
echo $this->formElementErrors($element, ['class' => 'help-inline']);
// Result:
// <ul class="help-inline"><li>Value is required and can&#039;t be empty</li></ul>

/**
 * Example #3: Custom format
 */
$helper = $this->formElementErrors();
$helper->setMessageOpenFormat('<div class="help-inline">');
$helper->setMessageSeparatorString('</div><div class="help-inline">');
$helper->setMessageCloseString('</div>');

echo $helper->render($element);
// Result: <div class="help-inline">Value is required and can&#039;t be empty</div>
```

## Public methods

The following public methods are in addition to those inherited from the
[AbstractHelper](abstract-helper.md#public-methods):

Method signature                                                     | Description
-------------------------------------------------------------------- | -----------
`setMessageOpenFormat(string $messageOpenFormat) : void`             | Set the (`printf`) formatted string used to open message representation; uses `<ul%s><li>` by default; attributes are inserted for the placeholder.
`getMessageOpenFormat() : string`                                    | Returns the formatted string used to open message representation.
`setMessageSeparatorString(string $messageSeparatorString) : string` | Sets the string used to separate messages; defaults to `</li><li>`.
`getMessageSeparatorString() : string`                               | Returns the string used to separate messages.
`setMessageCloseString(string $messageCloseString) : void`           | Sets the string used to close message representation; defaults to `</li></ul>`.
`getMessageCloseString() : string`                                   | Returns the string used to close message representation.
`setAttributes(array $attributes) : void`                            | Set the attributes that will go on the message open format as key/value pairs.
`getAttributes() : array`                                            | Returns the attributes that will go on the message open format.
`setTranslateMessages(bool $flag) : self`                            | Indicate whether or not element validation error messages should be translated during `render()`. Default is to translate them.
`render(ElementInterface $element [, array $attributes = array()]) : string` | Renders validation errors for the provided `$element`. Attributes provided will be used in the `messageOpenFormat`, and merged with any provided previously via `setAttributes()`.
