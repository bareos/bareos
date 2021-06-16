# Search

`Zend\Form\Element\Search` is meant to be paired with the
[FormSearch](../helper/form-search.md) helper for
[HTML5 inputs with type "search"](http://www.whatwg.org/specs/web-apps/current-work/multipage/states-of-the-type-attribute.html#text-%28type=text%29-state-and-search-state-%28type=search%29)

**Available since 2.9.0.**

## Basic Usage

This element automatically adds a `type` attribute of value `search`.

```php
use Zend\Form\Element;
use Zend\Form\Form;

$search = new Element\Search('search');
$search->setLabel('Search');

$form = new Form('my-form');
$form->add($search);
```

Using array notation:

```php
use Zend\Form\Element;
use Zend\Form\Form;

$form = new Form('my-form');
$form->add([
	'type' => Element\Search::class,
	'name' => 'search',
	'options' => [
		'label' => 'Search',
	],
]);
```

## Public Methods

This element defines no methods beyond those of the parent `Element` class.
