# FormSubmit

The `FormSubmit` view helper can be used to render an `<input type="submit">`
HTML form input. It is meant to work with the [Submit element](../element/submit.md).

## Basic usage

```php
use Zend\Form\Element;

$element = new Element\Submit('my-submit');

// Within your view...
echo $this->formSubmit($element);
```

Output:

```html
<input type="submit" name="my-submit" value="">
```
