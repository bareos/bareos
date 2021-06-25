# FormEmail

The `FormEmail` view helper can be used to render a `<input type="email">` HTML5
form input. It is meant to work with the [Email element](../element/email.md),
which provides a default input specification with an email validator.

## Basic usage

```php
use Zend\Form\Element;

$element = new Element\Email('my-email');

// Within your view...

echo $this->formEmail($element);
// Result: <input type="email" name="my-email" value="">
```
