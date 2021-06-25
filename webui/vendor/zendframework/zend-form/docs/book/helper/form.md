# Form

The `Form` view helper is used to render a `<form>` HTML element and its
attributes.

If provided a `Zend\Form\Form` instance, it will:

- `prepare()` the instance
- iterate it, and:
  - pass non-collection `Zend\Form\Element` instances to the `FormRow` helper to
    render, and
  - pass collections and fieldsets to the `FormCollection` helper to render.

For more fine-grained control, us the `form()` helper only for emitting the
opening and closing `<form>` tags, and manually use other helpers to render the
individual elements and fieldsets.

## Basic usage

```php
/**
 * inside a view template
 *
 * @var \Zend\View\Renderer\PhpRenderer $this
 * @var \Zend\Form\Form $form
 */

echo $this->form($form);
```

might result in markup like the following:

```html
<form action="" method="POST">
   <label>
      <span>Some Label</span>
      <input type="text" name="some_element" value="">
   </label>
</form>
```

## Public methods

The following public methods are in addition to those inherited from the
[AbstractHelper](abstract-helper.md#public-methods):

Method signature                                | Description
----------------------------------------------- | -----------
`__invoke(FormInterface $form = null) : string` | Prepares and renders the whole form.
`openTag(FormInterface $form = null) : string`  | Renders the `<form>` open tag for the `$form` instance.
`closeTag() : string`                           | Renders a `</form>` closing tag.
