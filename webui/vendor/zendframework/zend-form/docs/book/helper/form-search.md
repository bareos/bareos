# FormSearch

The `FormSearch` view helper can be used to render an `<input type="search">`
HTML5 form input.

`FormSearch` extends from [FormText](form-text.md).

## Basic usage

```php
use Zend\Form\Element;

$element = new Element('my-search');

// Within your view...
echo $this->formSearch($element);
```

Output:

```html
<input type="search" name="my-search" value="">
```
