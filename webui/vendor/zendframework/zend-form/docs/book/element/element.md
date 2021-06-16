# Element Base Class

`Zend\Form\Element` is a base class for all specialized elements and
`Zend\Form\Fieldset`.

## Basic Usage

At the bare minimum, each element or fieldset requires a name. You will also
typically provide some attributes to hint to the view layer how it might render
the item.

```php
use Zend\Form\Element;
use Zend\Form\Form;

$username = new Element\Text('username');
$username->setLabel('Username');
$username->setAttributes([
    'class' => 'username',
    'size'  => '30',
]);

$password = new Element\Password('password');
$password->setLabel('Password')
$password->setAttributes([
    'size'  => '30',
]);

$form = new Form('my-form');
$form->add($username);
$form->add($password);
```

## Public Methods

Method signature                                    | Description
--------------------------------------------------- | -----------
`setName(string $name) : void`                      | Set the name for this element.
`getName() : string`                                | Return the name for this element.
`setValue(string $value) : void`                    | Set the value for this element.
`getValue() : string`                               | Return the value for this element.
`setLabel(string $label) : void`                    | Set the label content for this element.
`getLabel() : string`                               | Return the label content for this element.
`setLabelAttributes(array $labelAttributes) : void` | Set the attributes to use with the label.
`getLabelAttributes() : array`                      | Return the attributes to use with the label.
`setLabelOptions(array $labelOptions) : void`       | Set label specific options.
`getLabelOptions() : array`                         | Return the label specific options.
`setOptions(array $options) : void`                 | Set options for an element. Accepted options are: ``label``, ``label_attributes"``, ``label_options``, which call ``setLabel``, ``setLabelAttributes`` and ``setLabelOptions``, respectively.
`getOptions() : array`                              | Get defined options for an element
`getOption(string $option) : null|mixed`            | Return the specified option, if defined. If it's not defined, returns null.
`setAttribute(string $key, mixed $value) : void`    | Set a single element attribute.
`getAttribute(string $key) : mixed`                 | Retrieve a single element attribute.
`removeAttribute(string $key) : void`               | Remove a single attribute
`hasAttribute(string $key) : boolean`               | Check if a specific attribute exists for this element.
`setAttributes(array|Traversable $arrayOrTraversable) : void` | Set many attributes at once. Implementation will decide if this will overwrite or merge.
`getAttributes() : array|Traversable`               | Retrieve all attributes at once.
`removeAttributes(array $keys) : void`              | Remove many attributes at once
`clearAttributes() : void`                          | Clear all attributes for this element.
`setMessages(array|Traversable $messages) : void`   | Set a list of messages to report when validation fails.
`getMessages() : array|Traversable`                 | Returns a list of validation failure messages, if any.
