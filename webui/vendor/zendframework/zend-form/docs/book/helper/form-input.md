# FormInput

The `FormInput` view helper is used to render an `<input>` HTML form input tag.
It acts as a base class for all of the specifically typed form input helpers
(`FormText`, `FormCheckbox`, `FormSubmit`, etc.), and is not suggested for
direct use.

It contains a general map of valid tag attributes and types for attribute
filtering. Each subclass of `FormInput` implements its own specific map of valid
tag attributes.

## Public methods

The following public methods are in addition to those inherited from the
[AbstractHelper](abstract-helper.md#public-methods):

Method signature                             | Description
-------------------------------------------- | -----------
`render(ElementInterface $element) : string` | Renders the `<input>` tag for the `$element`.
