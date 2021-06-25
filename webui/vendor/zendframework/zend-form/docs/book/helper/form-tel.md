# FormTel

The `FormTel` view helper can be used to render an `<input type="tel">` HTML5
form input.

## Basic usage

```php
use Zend\Form\Element;

$element = new Element('my-tel');

// Within your view...
echo $this->formTel($element);
```

Output:

```html
<input type="tel" name="my-tel" value="">
```
