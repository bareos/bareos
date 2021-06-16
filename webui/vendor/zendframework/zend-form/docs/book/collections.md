# Form Collections

Often, fieldsets or elements in your forms will correspond to other domain
objects. In some cases, they may correspond to *collections* of domain objects.
In this latter case, in terms of user interfaces, you may want to add items
dynamically in the user interface &mdash; a great example is adding tasks to a
task list.

This document is intended to demonstrate these features. To do so, we first need
to define some domain objects that we'll be using.

```php
namespace Application\Entity;

class Product
{
    /**
     * @var string
     */
    protected $name;

    /**
     * @var int
     */
    protected $price;

    /**
     * @var Brand
     */
    protected $brand;

    /**
     * @var array
     */
    protected $categories;

    /**
     * @param string $name
     * @return self
     */
    public function setName($name)
    {
        $this->name = $name;
        return $this;
    }

    /**
     * @return string
     */
    public function getName()
    {
        return $this->name;
    }

    /**
     * @param int $price
     * @return self
     */
    public function setPrice($price)
    {
        $this->price = $price;
        return $this;
    }

    /**
     * @return int
     */
    public function getPrice()
    {
        return $this->price;
    }

    /**
     * @param Brand $brand
     * @return self
     */
    public function setBrand(Brand $brand)
    {
        $this->brand = $brand;
        return $this;
    }

    /**
     * @return Brand
     */
    public function getBrand()
    {
        return $this->brand;
    }

    /**
     * @param Category[] $categories
     * @return self
     */
    public function setCategories(array $categories)
    {
        $this->categories = $categories;
        return $this;
    }

    /**
     * @return Category[]
     */
    public function getCategories()
    {
        return $this->categories;
    }
}

class Brand
{
    /**
     * @var string
     */
    protected $name;

    /**
     * @var string
     */
    protected $url;

    /**
     * @param string $name
     * @return self
     */
    public function setName($name)
    {
        $this->name = $name;
        return $this;
    }

    /**
     * @return string
     */
    public function getName()
    {
        return $this->name;
    }

    /**
     * @param string $url
     * @return self
     */
    public function setUrl($url)
    {
        $this->url = $url;
        return $this;
    }

    /**
     * @return string
     */
    public function getUrl()
    {
        return $this->url;
    }
}

class Category
{
    /**
     * @var string
     */
    protected $name;

    /**
     * @param string $name
     * @return self
     */
    public function setName($name)
    {
        $this->name = $name;
        return $this;
    }

    /**
     * @return string
     */
    public function getName()
    {
        return $this->name;
    }
}
```

A `Product` has two scalar properties, name and price, a
1:1 relationship (one `Product` has one `Brand`), and a 1:N relationship (one
`Product` has many `Category` instances).

## Creating Fieldsets

The first step is to create three fieldsets. Each fieldset will contain all the
fields and relationships for a specific entity.

Here is the `Brand` fieldset:

```php
namespace Application\Form;

use Application\Entity\Brand;
use Zend\Form\Element;
use Zend\Form\Fieldset;
use Zend\InputFilter\InputFilterProviderInterface;
use Zend\Hydrator\ClassMethods as ClassMethodsHydrator;

class BrandFieldset extends Fieldset implements InputFilterProviderInterface
{
    public function __construct()
    {
        parent::__construct('brand');

        $this->setHydrator(new ClassMethodsHydrator(false));
        $this->setObject(new Brand());

        $this->add([
            'name' => 'name',
            'options' => [
                'label' => 'Name of the brand',
            ],
            'attributes' => [
                'required' => 'required',
            ],
        ]);

        $this->add([
            'name' => 'url',
            'type' => Element\Url::class,
            'options' => [
                'label' => 'Website of the brand',
            ],
            'attributes' => [
                'required' => 'required',
            ],
        ]);
    }

    /**
     * @return array
     */
    public function getInputFilterSpecification()
    {
        return [
            'name' => [
                'required' => true,
            ],
        ];
    }
}
```

The fieldset calls the method `setHydrator()`, giving it a `ClassMethods`
hydrator, and the `setObject()` method, giving it an empty `Brand` instance.

When we validate incoming data, the form will automatically iterate through all
the fieldsets it contains populate all sub-objects, in order to return a
complete entity.

Notice that the `Url` element has a type of `Zend\Form\Element\Url`. This
information will be used to validate the input field. You don't need to manually
add filters or validators for this input as that element provides a reasonable
input specification.

Finally, `getInputFilterSpecification()` gives the specification for the
remaining input ("name"), indicating that this input is required. Note that
*required* in the array "attributes" (when elements are added) is only meant to
add the "required" attribute to the form markup (and therefore has semantic
meaning only).

Here is the `Category` fieldset:

```php
namespace Application\Form;

use Application\Entity\Category;
use Zend\Form\Fieldset;
use Zend\InputFilter\InputFilterProviderInterface;
use Zend\Hydrator\ClassMethods as ClassMethodsHydrator;

class CategoryFieldset extends Fieldset implements InputFilterProviderInterface
{
    public function __construct()
    {
        parent::__construct('category');

        $this->setHydrator(new ClassMethodsHydrator(false));
        $this->setObject(new Category());

        $this->setLabel('Category');

        $this->add([
            'name' => 'name',
            'options' => [
                'label' => 'Name of the category',
            ],
            'attributes' => [
                'required' => 'required',
            ],
        ]);
    }

    /**
     * @return array
     */
    public function getInputFilterSpecification()
    {
        return [
            'name' => [
                'required' => true,
            ],
        ];
    }
}
```

Nothing new here.

Finally, the `Product` fieldset:

```php
namespace Application\Form;

use Application\Entity\Product;
use Zend\Form\Element;
use Zend\Form\Fieldset;
use Zend\InputFilter\InputFilterProviderInterface;
use Zend\Hydrator\ClassMethods as ClassMethodsHydrator;

class ProductFieldset extends Fieldset implements InputFilterProviderInterface
{
    public function __construct()
    {
        parent::__construct('product');

        $this->setHydrator(new ClassMethodsHydrator(false));
        $this->setObject(new Product());

        $this->add([
            'name' => 'name',
            'options' => [
                'label' => 'Name of the product',
            ],
            'attributes' => [
                'required' => 'required',
            ],
        ]);

        $this->add([
            'name' => 'price',
            'options' => [
                'label' => 'Price of the product',
            ],
            'attributes' => [
                'required' => 'required',
            ],
        ]);

        $this->add([
            'type' => BrandFieldset::class,
            'name' => 'brand',
            'options' => [
                'label' => 'Brand of the product',
            ],
        ]);

        $this->add([
            'type' => Element\Collection::class,
            'name' => 'categories',
            'options' => [
                'label' => 'Please choose categories for this product',
                'count' => 2,
                'should_create_template' => true,
                'allow_add' => true,
                'target_element' => [
                    'type' => CategoryFieldset::class,
                ],
            ],
        ]);
    }

    /**
     * Should return an array specification compatible with
     * {@link Zend\InputFilter\Factory::createInputFilter()}.
     *
     * @return array
     */
    public function getInputFilterSpecification()
    {
        return [
            'name' => [
                'required' => true,
            ],
            'price' => [
                'required' => true,
                'validators' => [
                    [
                        'name' => 'Float',
                    ],
                ],
            ],
        ];
    }
}
```

We have a lot of new things here!

First, notice how the brand element is added: we specify it to be of type
`Application\Form\BrandFieldset`. This is how you handle a 1:1
relationship. When the form is validated, the `BrandFieldset` will first be
populated, and will return a `Brand` entity (as we have specified a
`ClassMethods` hydrator, and bound the fieldset to a `Brand` entity using the
`setObject()` method). This `Brand` entity will then be used to populate the
`Product` entity by calling the `setBrand()` method.

The next element shows you how to handle 1:N relationships. The type is
`Zend\Form\Element\Collection`, which is a specialized element to handle such
cases. As you can see, the name of the element ("categories") perfectly matches
the name of the property in the `Product` entity.

This element has a few interesting options:

- `count`: this is how many times the element (in this case, a category) has to
  be rendered. We've set it to `2` in this example.
- `should_create_template`: if set to `true`, it will generate a template markup
  in a `<span>` element, in order to simplify adding new element on the fly (we
  will speak about this one later).
- `allow_add`: if set to `true` (which is the default), dynamically added
  elements will be retrieved and validated; otherwise, they will be completely
  ignored. This, of course, depends on what you want to do.
- `target_element`: this is either an element or, as this is the case in this
  example, an array that describes the element or fieldset that will be used in
  the collection. In this case, the `target_element` is a `Category` fieldset.

## The Form Element

So far, so good. We now have our fieldsets in place. But those are fieldsets,
not forms. And only `Form` instances can be validated. So here is the form:

```php
namespace Application\Form;

use Zend\Form\Element;
use Zend\Form\Form;
use Zend\InputFilter\InputFilter;
use Zend\Hydrator\ClassMethods as ClassMethodsHydrator;

class CreateProduct extends Form
{
    public function __construct()
    {
        parent::__construct('create_product');

        $this->setAttribute('method', 'post')
        $this->setHydrator(new ClassMethodsHydrator(false));
        $this->setInputFilter(new InputFilter());

        $this->add([
            'type' => ProductFieldset::class,
            'options' => [
                'use_as_base_fieldset' => true,
            ],
        ]);

        $this->add([
            'type' => Element\Csrf::class,
            'name' => 'csrf',
        ]);

        $this->add([
            'name' => 'submit',
            'attributes' => [
                'type' => 'submit',
                'value' => 'Send',
            ],
        ]);
    }
}
```

`CreateProduct` only defines a `Product` fieldset, as well as some other useful
fields (`CSRF` for security, and a `Submit` button).

Notice the `use_as_base_fieldset` option. This option is here to say to the
form: "hey, the object I bind to you is, in fact, bound to the fieldset that is
the base fieldset." This will be to true in most situations.

This approach allows each entity to have its own `Fieldset`, and enables re-use.
You describe the elements, the filters, and validators for each entity only
once, and the concrete `Form` instance will only compose those fieldsets. You no
longer have to add the "username" input to every form that deals with users!

## Integrating with zend-mvc

Now, let's create a zend-mvc controller action:

```php
/**
  * @return array
  */
 public function indexAction()
 {
     $form = new CreateProduct();
     $product = new Product();
     $form->bind($product);
     $request = $this->getRequest();

     if ($request->isPost()) {
         $form->setData($request->getPost());

         if ($form->isValid()) {
            var_dump($product);
         }
     }

     return [
         'form' => $form,
     ];
 }
```

All the controller does is:

- Create a form instance.
- Bind it to an object.
- Validate it and/or return the form instance for later use by the view.

## Integration with zend-view

And finally, the view:

```php
<?php
$form->setAttribute('action', $this->url('home'));
$form->prepare();
echo $this->form()->openTag($form);

$product = $form->get('product');

// The name and price can be rendered as regular elements:
echo $this->formRow($product->get('name'));
echo $this->formRow($product->get('price'));

// Categories are rendered as a collection:
echo $this->formCollection($product->get('categories'));

$brand = $product->get('brand');

// Since the brand is a 1:1 relationship, its elements are rendered normally:
echo $this->formRow($brand->get('name'));
echo $this->formRow($brand->get('url'));

// And finally, we render the CSRF and submit elements:
echo $this->formHidden($form->get('csrf'));
echo $this->formElement($form->get('submit'));

echo $this->form()->closeTag();
```

A few new things here :

- the `prepare()` method. You *must* call it prior to rendering anything in the
  view (this function is only meant to be called in views, not in controllers).
- the `FormRow` helper renders a label (if present), the input itself, and
  errors.
- the `FormCollection` helper will iterate through every element in the
  collection, and render every element with the `FormRow` helper (you may
  specify an alternate helper if desired, using the `setElementHelper()` method
  on that `FormCollection` helper instance). If you need more control about the
  way you render your forms, you can iterate through the elements in the
  collection, and render them manually one by one.

Here is the result:

![Rendered collection](images/collections.view.png)

Collections are wrapped inside a fieldset, and every item in the collection is
itself wrapped in the fieldset. In fact, the `Collection` element uses `label`
for each item in the collection, while the label of the `Collection` element
itself is used as the `legend` of the `fieldset`. You must have a label on every
element in order to use this feature. If you don't want the fieldset created,
but just the elements within it, add a boolean `false` as the second parameter
of the `FormCollection` view helper.

If you validate, all elements will show errors (this is normal, as we've marked
them as required). As soon as the form is valid, this is what we get :

![Valid collection result](images/collections.view.result.png)

The bound object is completely filled with the object instances we specified,
not with arrays!

## Adding New Elements Dynamically

Remember that `should_create_template` flag? We're going to use it now.

Often, forms are not completely static. In our case, let's say that we don't
want only two categories, but we want the user to be able to add more at
runtime. zend-form enables this capability. First, let's see what it generates
when we ask it to create a template:

![Collection dynamic elements template](images/collections.dynamic-elements.template.png)

The collection generates two fieldsets (the two categories) *plus* a span with a
`data-template` attribute that contains the full HTML code to copy to create a
new element in the collection. The value `__index__` (this is the placeholder
generated) must be changed to a valid value. Currently, we have 2 elements
(`categories[0]` and `categories[1]`, so `__index__` must be changed to 2.

If you want, this placeholder (`__index__` is the default) can be changed using
the `template_placeholder` option key:

```php
$this->add([
    'type' => Element\Collection::class,
    'name' => 'categories',
    'options' => [
        'label' => 'Please choose categories for this product',
        'count' => 2,
        'should_create_template' => true,
        'template_placeholder' => '__placeholder__',
        'target_element' => [
            'type' => CategoryFieldset::class,
        ],
    ],
]);
```

First, let's add a small button "Add new category" anywhere in the form:

```html
<button onclick="return add_category()">Add a new category</button>
```

The `add_category` function will need to do the following:

1. First, count the number of elements we already have.
2. Get the template from the `span`'s `data-template` attribute.
3. Change the placeholder to a valid index.
4. Add the element to the DOM.

The following is a potential implementation:

```javascript
function add_category() {
    var currentCount = $('form > fieldset > fieldset').length;
    var template = $('form > fieldset > span').data('template');
    template = template.replace(/__index__/g, currentCount);

    $('form > fieldset').append(template);

    return false;
}
```

(Note: the above example assumes `$()` is defined, and equivalent to jQuery's
`$()` function, Dojo's `dojo.query`, etc.)

One small remark about `template.replace`: the example uses `currentCount` and
not `currentCount + 1`, as the indices are zero-based (so, if we have two
elements in the + collection, the third one will have the index `2`).

Now if we validate the form, it will automatically take into account this new
element by validating it, filtering it and retrieving it:

![Validated collection](images/collections.dynamic-elements.result.png)

If you don't want to allow adding elements in a collection, you must set the
option `allow_add` to `false`. This way, even if new elements are added, they
won't be validated and hence, not added to the entity. Also, if we don't want
elements to be added, we don't need the data template, either.

Here's how you both disable adding dynamic elements, and disable the template:

```php
$this->add([
    'type' => Element\Collection::class,
    'name' => 'categories',
    'options' => [
        'label' => 'Please choose categories for this product',
        'count' => 2,
        // Do not allow adding:
        'allow_add' => false,
        // Do not display the index template:
        'should_create_template' => false,
        'target_element' => [
            'type' => CategoryFieldset::class,
        ],
    ],
]);
```

There are some limitations to this capability:

- Although you can add new elements and remove them, you *CANNOT* remove more
  elements in a collection than the initial count (for instance, if your code
  specifies `count == 2`, you will be able to add a third one and remove it, but
  you won't be able to remove any others. If the initial count is 2, you *must*
  have at least two elements.
- Dynamically added elements have to be added at the end of the collection. They
  can be added anywhere (these elements will still be validated and inserted
  into the entity), but if the validation fails, this newly added element will
  be automatically placed at the end of the collection.

## Validation groups for fieldsets and collection

Validation groups allow you to validate a subset of fields.

As an example, although the `Brand` entity has a `url` property, we don't want
the user to specify it in the creation form (but may wish to later in the "Edit
Product" form, for instance). Let's update the view to remove the `url` input:

```php
<?php
$form->setAttribute('action', $this->url('home'));
$form->prepare();

echo $this->form()->openTag($form);

$product = $form->get('product');

echo $this->formRow($product->get('name'));
echo $this->formRow($product->get('price'));
echo $this->formCollection($product->get('categories'));

$brand = $product->get('brand');

echo $this->formRow($brand->get('name'));

echo $this->formHidden($form->get('csrf'));
echo $this->formElement($form->get('submit'));

echo $this->form()->closeTag();
```

This is what we get:

![Collection validation group](images/collections.validation-groups.png)

The `url` input has disappeared, but even if we fill every input, the form won't
validate. In fact, this is normal. We specified in the input filter that the
`url` is a *required* field, so if the form does not have it, it won't validate,
even though we didn't add it to the view!

Of course, you could create a `BrandFieldsetWithoutUrl` fieldset, but this would
require a lot of duplicate code.

The solution: validation groups. A validation group is specified in a `Form`
object (hence, in our case, in the `CreateProduct` form) by giving an array of
all the elements we want to validate. Our `CreateProduct` class now looks like
this:

```php
namespace Application\Form;

use Zend\Form\Element;
use Zend\Form\Form;
use Zend\InputFilter\InputFilter;
use Zend\Hydrator\ClassMethods as ClassMethodsHydrator;

class CreateProduct extends Form
{
    public function __construct()
    {
        parent::__construct('create_product');

        $this->setAttribute('method', 'post');
        $this->setHydrator(new ClassMethodsHydrator());
        $this->setInputFilter(new InputFilter());

        $this->add([
            'type' => ProductFieldset::class,
            'options' => [
                'use_as_base_fieldset' => true,
            ],
        ]);

        $this->add([
            'type' => Element\Csrf::class,
            'name' => 'csrf',
        ]);

        $this->add([
            'name' => 'submit',
            'attributes' => [
                'type' => 'submit',
                'value' => 'Send',
            ],
        ]);

        $this->setValidationGroup([
            'csrf',
            'product' => [
                'name',
                'price',
                'brand' => [
                    'name',
                ],
                'categories' => [
                    'name',
                ],
            ],
        ]);
    }
}
```

Don't forget to add the `CSRF` element, as we want it to be validated too (but
notice that the example omits the `submit` element, as we don't care about its
value). You can also recursively select the elements if desired.

There is one limitation currently: validation groups for collections are set on
a per-collection basis, not per-element in a collection basis. This means you
cannot say, "validate the name input for the first element of the categories
collection, but don't validate it for the second one."

Now, the form validates (and the `url` is set to null as we didn't specify it).

## Preventing validation from wiping out previous collection items

In some cases, you may be representing collections within a model, but not
validating them; as an example, if you use a validation group that excludes the
collections from validation so that they remain untouched after binding.

Starting in 2.8.4, behavior around collections changed in order to fix some
underlying bugs. One such change is that if a collection is found in a form, but
has no associated data, an empty array is assigned to it, even when not in the
validation group. This effectively wipes out the collection data when you bind
values.

To prevent this behavior, starting in 2.9.1 you may pass an optional second
argument to `bindValues()` on either a fieldset or collection,
`$validationGroup`; when present, these instances will first check if the
collection is in the validation group before binding the value; if it is not,
the collection will not be represented. The `Form` class has been updated to
pass the validation group, if present, on to fieldset and collection instances
when performing `bindValues()` operations.

For more details, refer to the following issues:

- [zendframework/zend-form#19](https://github.com/zendframework/zend-form/pull/19)
- [zendframework/zend-form#102](https://github.com/zendframework/zend-form/pull/102)
- [zendframework/zend-form#103](https://github.com/zendframework/zend-form/pull/103)
- [zendframework/zend-form#106](https://github.com/zendframework/zend-form/pull/106)
