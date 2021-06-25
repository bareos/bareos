# FormPassword

The `FormPassword` view helper can be used to render an `<input
type="password">` HTML form input. It is meant to work with the
[Password element](../element/password.md).

## Basic usage

```php
use Zend\Form\Element;

$element = new Element\Password('my-password');

// Within your view...
echo $this->formPassword($element);
```

Output:

```html
<input type="password" name="my-password" value="">
```
