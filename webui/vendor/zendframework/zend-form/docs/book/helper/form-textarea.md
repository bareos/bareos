# FormTextarea

The `FormTextarea` view helper can be used to render a `<textarea></textarea>`
HTML form input. It is meant to work with the [Textarea element](../element/textarea.md).

## Basic usage

```php
use Zend\Form\Element;

$element = new Element\Textarea('my-textarea');

// Within your view...
echo $this->formTextarea($element);
```

Output:

```html
<textarea name="my-textarea"></textarea>
```
