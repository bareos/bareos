# Quick Start

Forms are composed of elements and fieldsets. At the bare minimum, each element
or fieldset requires a name; in most situations, you'll also provide some
attributes to hint to the view layer how it might render the item. The form
itself generally composes an `InputFilter` &mdash; which you can also create
directly in the form via a factory. Individual elements can hint as to what
defaults to use when generating a related input for the input filter.

Perform form validation by providing an array of data to the `setData()` method,
and calling the `isValid()` method. If you want to simplify your work even more,
you can bind an object to the form; on successful validation, it will be
populated from the validated values.

## Programmatic Form Creation

The following example demonstrates element, fieldset, and form creation, and how
they are wired together.

```php
use Zend\Captcha;
use Zend\Form\Element;
use Zend\Form\Fieldset;
use Zend\Form\Form;
use Zend\InputFilter\Input;
use Zend\InputFilter\InputFilter;

// Create a text element to capture the user name:
$name = new Element('name');
$name->setLabel('Your name');
$name->setAttributes([
    'type' => 'text',
]);

// Create a text element to capture the user email address:
$email = new Element\Email('email');
$email->setLabel('Your email address');

// Create a text element to capture the message subject:
$subject = new Element('subject');
$subject->setLabel('Subject');
$subject->setAttributes([
    'type' => 'text',
]);

// Create a textarea element to capture a message:
$message = new Element\Textarea('message');
$message->setLabel('Message');

// Create a CAPTCHA:
$captcha = new Element\Captcha('captcha');
$captcha->setCaptcha(new Captcha\Dumb());
$captcha->setLabel('Please verify you are human');

// Create a CSRF token:
$csrf = new Element\Csrf('security');

// Create a submit button:
$send = new Element('send');
$send->setValue('Submit');
$send->setAttributes([
    'type' => 'submit',
]);

// Create the form and add all elements:
$form = new Form('contact');
$form->add($name);
$form->add($email);
$form->add($subject);
$form->add($message);
$form->add($captcha);
$form->add($csrf);
$form->add($send);

// Create an input for the "name" element:
$nameInput = new Input('name');

/* ... configure the input, and create and configure all others ... */

// Create the input filter:
$inputFilter = new InputFilter();

// Attach inputs:
$inputFilter->add($nameInput);
/* ... */

// Attach the input filter to the form:
$form->setInputFilter($inputFilter);
```

As a demonstration of fieldsets, let's alter the above slightly. We'll create
two fieldsets, one for the sender information, and another for the message
details.

```php
// Create the fieldset for sender details:
$sender = new Fieldset('sender');
$sender->add($name);
$sender->add($email);

// Create the fieldset for message details:
$details = new Fieldset('details');
$details->add($subject);
$details->add($message);

$form = new Form('contact');
$form->add($sender);
$form->add($details);
$form->add($captcha);
$form->add($csrf);
$form->add($send);
```

This manual approach gives maximum flexibility over form creation; however, it
comes at the expense of verbosity. In the next section, we'll look at another
approach.

## Creation via Factory

You can create the entire form and input filter at once using the `Factory`.
This is particularly nice if you want to store your forms as pure configuration;
you can then pass the configuration to the factory and be done.

```php
use Zend\Form\Element;
use Zend\Form\Factory;
use Zend\Hydrator\ArraySerializable;

$factory = new Factory();
$form    = $factory->createForm([
    'hydrator' => ArraySerializable::class,
    'elements' => [
        [
            'spec' => [
                'name' => 'name',
                'options' => [
                    'label' => 'Your name',
                ],
                'type'  => 'Text',
            ],
        ],
        [
            'spec' => [
                'type' => Element\Email::class,
                'name' => 'email',
                'options' => [
                    'label' => 'Your email address',
                ]
            ],
        ],
        [
            'spec' => [
                'name' => 'subject',
                'options' => [
                    'label' => 'Subject',
                ],
                'type'  => 'Text',
            ],
        ],
        [
            'spec' => [
                'type' => Element\Textarea::class,
                'name' => 'message',
                'options' => [
                    'label' => 'Message',
                ]
            ],
        ],
        [
            'spec' => [
                'type' => Element\Captcha::class,
                'name' => 'captcha',
                'options' => [
                    'label' => 'Please verify you are human.',
                    'captcha' => [
                        'class' => 'Dumb',
                    ],
                ],
            ],
        ],
        [
            'spec' => [
                'type' => Element\Csrf::class,
                'name' => 'security',
            ],
        ],
        [
            'spec' => [
                'name' => 'send',
                'type'  => 'Submit',
                'attributes' => [
                    'value' => 'Submit',
                ],
            ],
        ],
    ],

    /* If we had fieldsets, they'd go here; fieldsets contain
     * "elements" and "fieldsets" keys, and potentially a "type"
     * key indicating the specific FieldsetInterface
     * implementation to use.
    'fieldsets' => [
    ],
     */

    // Configuration to pass on to
    // Zend\InputFilter\Factory::createInputFilter()
    'input_filter' => [
        /* ... */
    ],
]);
```

If we wanted to use fieldsets, as we demonstrated in the previous example, we
could do the following:

```php
use Zend\Form\Element;
use Zend\Form\Factory;
use Zend\Hydrator\ArraySerializable;

$factory = new Factory();
$form    = $factory->createForm([
    'hydrator'  => ArraySerializable::class,

    // Top-level fieldsets to define:
    'fieldsets' => [
        [
            'spec' => [
                'name' => 'sender',
                'elements' => [
                    [
                        'spec' => [
                            'name' => 'name',
                            'options' => [
                                'label' => 'Your name',
                            ],
                            'type' => 'Text'
                        ],
                    ],
                    [
                        'spec' => [
                            'type' => Element\Email::class,
                            'name' => 'email',
                            'options' => [
                                'label' => 'Your email address',
                            ],
                        ],
                    ],
                ],
            ],
        ],
        [
            'spec' => [
                'name' => 'details',
                'elements' => [
                    [
                        'spec' => [
                            'name' => 'subject',
                            'options' => [
                                'label' => 'Subject',
                            ],
                            'type' => 'Text',
                        ],
                    ],
                    [
                        'spec' => [
                            'name' => 'message',
                            'type' => Element\Textarea::class,
                            'options' => [
                                'label' => 'Message',
                            ],
                        ],
                    ],
                ],
            ],
        ],
    ],

    // You can specify an "elements" key explicitly:
    'elements' => [
        [
            'spec' => [
                'type' => Element\Captcha::class,
                'name' => 'captcha',
                'options' => [
                    'label' => 'Please verify you are human.',
                    'captcha' => [
                        'class' => 'Dumb',
                    ],
                ],
            ],
        ],
        [
            'spec' => [
            'type' => Element\Csrf::class,
            'name' => 'security',
        ],
    ],

    // But entries without string keys are also considered elements:
    [
        'spec' => [
            'name' => 'send',
            'type'  => 'Submit',
            'attributes' => [
                'value' => 'Submit',
            ],
        ],
    ],

    // Configuration to pass on to
    // Zend\InputFilter\Factory::createInputFilter()
    'input_filter' => [
        /* ... */
    ],
]);
```

Note that the chief difference is nesting; otherwise, the information is
basically the same.

The chief benefits to using the `Factory` are allowing you to store definitions
in configuration, and usage of significant whitespace.

## Factory-backed Form Extension

The default `Form` implementation is backed by the `Factory`. This allows you to
extend it, and define your form internally. This has the benefit of allowing a
mixture of programmatic and factory-backed creation, as well as defining a form
for re-use in your application.

```php
namespace Contact;

use Zend\Captcha\AdapterInterface as CaptchaAdapter;
use Zend\Form\Element;
use Zend\Form\Form;

class ContactForm extends Form
{
    protected $captcha;

    public function __construct(CaptchaAdapter $captcha)
    {
        parent::__construct();

        $this->captcha = $captcha;

        // add() can take an Element/Fieldset instance, or a specification, from
        // which the appropriate object will be built.
        $this->add([
            'name' => 'name',
            'options' => [
                'label' => 'Your name',
            ],
            'type'  => 'Text',
        ]);
        $this->add([
            'type' => Element\Email::class,
            'name' => 'email',
            'options' => [
                'label' => 'Your email address',
            ],
        ]);
        $this->add([
            'name' => 'subject',
            'options' => [
                'label' => 'Subject',
            ],
            'type'  => 'Text',
        ]);
        $this->add([
            'type' => Element\Textarea::class,
            'name' => 'message',
            'options' => [
                'label' => 'Message',
            ],
        ]);
        $this->add([
            'type' => Element\Captcha::class,
            'name' => 'captcha',
            'options' => [
                'label' => 'Please verify you are human.',
                'captcha' => $this->captcha,
            ],
        ]);
        $this->add(new Element\Csrf('security'));
        $this->add([
            'name' => 'send',
            'type'  => 'Submit',
            'attributes' => [
                'value' => 'Submit',
            ],
        ]);

        // We could also define the input filter here, or
        // lazy-create it in the getInputFilter() method.
    }
}
```

In the above example, elements are added in the constructor. This is done to
allow altering and/or configuring either the form or input filter factory
instances, which could then have bearing on how elements, inputs, etc. are
created. In this case, it also allows injection of the CAPTCHA adapter, allowing
us to configure it elsewhere in our application and inject it into the form.

## Validating Forms

Validating forms requires three steps. First, the form must have an input filter
attached. Second, you must inject the data to validate into the form. Third, you
validate the form. If invalid, you can retrieve the error messages, if any.

```php
// assuming $captcha is an instance of some Zend\Captcha\AdapterInterface:
$form = new Contact\ContactForm($captcha);

// If the form doesn't define an input filter by default, inject one.
$form->setInputFilter(new Contact\ContactFilter());

// Get the data. In an MVC application, you might try:
$data = $request->getPost();  // for POST data
$data = $request->getQuery(); // for GET (or query string) data

$form->setData($data);

// Validate the form
if ($form->isValid()) {
    $validatedData = $form->getData();
} else {
    $messages = $form->getMessages();
}
```

> ### Always populate select elements with options
>
> Always ensure that options for a select element are populated *prior* to
> validation; otherwise, the element will fail validation, and you will receive
> a `NotInArray` error message.
>
> If you are populating the options from a database or other data source, make
> sure this is done prior to validation. Alternately, you may disable the
> `InArray` validator programmatically prior to validation:
>
> ```php
> $element->setDisableInArrayValidator(true);
> ```

You can get the raw data if you want, by accessing the composed input filter.

```php
$filter = $form->getInputFilter();

$rawValues    = $filter->getRawValues();
$nameRawValue = $filter->getRawValue('name');
```

## Hinting to the Input Filter

Often, you'll create elements that you expect to behave in the same way on each
usage, and for which you'll want specific filters or validation as well. Since
the input filter is a separate object, how can you achieve these latter points?

Because the default form implementation composes a factory, and the default
factory composes an input filter factory, you can have your elements and/or
fieldsets hint to the input filter. If no input or input filter is provided in
the input filter for that element, these hints will be retrieved and used to
create them.

To do so, one of the following must occur. For elements, they must implement
`Zend\InputFilter\InputProviderInterface`, which defines a
`getInputSpecification()` method; for fieldsets (and, by extension, forms), they
must implement `Zend\InputFilter\InputFilterProviderInterface`, which defines a
`getInputFilterSpecification()` method.

In the case of an element, the `getInputSpecification()` method should return
data to be used by the input filter factory to create an input. Every HTML5
(`email`, `url`, `color`, etc.) element has a built-in element that uses this
logic. For instance, here is how the `Zend\Form\Element\Color` element is
defined:

```php
namespace Zend\Form\Element;

use Zend\Filter;
use Zend\Form\Element;
use Zend\InputFilter\InputProviderInterface;
use Zend\Validator\Regex as RegexValidator;
use Zend\Validator\ValidatorInterface;

class Color extends Element implements InputProviderInterface
{
    /**
     * Seed attributes
     *
     * @var array
     */
    protected $attributes = [
        'type' => 'color',
    ];

    /**
     * @var ValidatorInterface
     */
    protected $validator;

    /**
     * Get validator
     *
     * @return ValidatorInterface
     */
    protected function getValidator()
    {
        if (null === $this->validator) {
            $this->validator = new RegexValidator('/^#[0-9a-fA-F]{6}$/');
        }
        return $this->validator;
    }

    /**
     * Provide default input rules for this element
     *
     * Attaches an email validator.
     *
     * @return array
     */
    public function getInputSpecification()
    {
        return [
            'name' => $this->getName(),
            'required' => true,
            'filters' => [
                ['name' => Filter\StringTrim::class],
                ['name' => Filter\StringToLower::class],
            ],
            'validators' => [
                $this->getValidator(),
            ],
        ];
    }
}
```

The above hints to the input filter to create and attach an input named after
the element, marking it as required, giving it `StringTrim` and `StringToLower`
filters, and defining a `Regex` validator. Note that you can either rely on the
input filter to create filters and validators, or directly instantiate them.

For fieldsets, you do very similarly; the difference is that
`getInputFilterSpecification()` must return configuration for an input filter.

```php
namespace Contact\Form;

use Zend\Filter;
use Zend\Form\Fieldset;
use Zend\InputFilter\InputFilterProviderInterface;
use Zend\Validator;

class SenderFieldset extends Fieldset implements InputFilterProviderInterface
{
    public function getInputFilterSpecification()
    {
        return [
            'name' => [
                'required' => true,
                'filters'  => [
                    ['name' => Filter\StringTrim::class],
                ],
                'validators' => [
                    [
                        'name' => Validator\StringLength::class,
                        'options' => [
                            'min' => 3,
                            'max' => 256
                        ],
                    ],
                ],
            ],
            'email' => [
                'required' => true,
                'filters'  => [
                    ['name' => Filter\StringTrim::class],
                ],
                'validators' => [
                    new Validator\EmailAddress(),
                ],
            ],
        ];
    }
}
```

Specifications are a great way to make forms, fieldsets, and elements re-usable
trivially in your applications. In fact, the `Captcha` and `Csrf` elements
define specifications in order to ensure they can work without additional user
configuration!

> ### Use the most specific input type
>
> If you set custom input filter specification either in
> `getInputSpecification()` or in `getInputFilterSpecification()`, the
> `Zend\InputFilter\InputInterface` set for that specific field is reset to the
> default `Zend\InputFilter\Input`.
>
> Some form elements may need a particular input filter, like
> `Zend\Form\Element\File`: in this case it's mandatory to specify the `type`
> key in your custom specification to match the original one (e.g., for the
> file element, use `Zend\InputFilter\FileInput`).

## Binding an object

As noted in the introduction, forms bridge the domain model and the view layer.
Let's see that in action.

When you `bind()` an object to the form, the following happens:

- The composed `Hydrator` calls `extract()` on the object, and uses the values
  returned, if any, to populate the `value` attributes of all elements. If a
  form contains a fieldset that itself contains another fieldset, the form will
  recursively extract the values.
- When `isValid()` is called, if `setData()` has not been previously set, the
  form uses the composed `Hydrator` to extract values from the object, and uses
  those during validation.
- If `isValid()` is successful (and the `bindOnValidate` flag is enabled, which
  is true by default), then the `Hydrator` will be passed the validated values
  to use to hydrate the bound object. (If you do not want this behavior, call
  `setBindOnValidate(FormInterface::BIND_MANUAL)`).
- If the object implements `Zend\InputFilter\InputFilterAwareInterface`, the
  input filter it composes will be used instead of the one composed on the form.

This is easier to understand with an example.

```php
$contact = new ArrayObject;
$contact['subject'] = '[Contact Form] ';
$contact['message'] = 'Type your message here';

$form = new Contact\ContactForm;

$form->bind($contact); // form now has default values for
                       // 'subject' and 'message'

$data = [
    'name'    => 'John Doe',
    'email'   => 'j.doe@example.tld',
    'subject' => '[Contact Form] \'sup?',
];
$form->setData($data);

if ($form->isValid()) {
    // $contact now has the following structure:
    // [
    //     'name'    => 'John Doe',
    //     'email'   => 'j.doe@example.tld',
    //     'subject' => '[Contact Form] \'sup?',
    //     'message' => 'Type your message here',
    // ]
    // But is an ArrayObject instance!
}
```

When an object is bound to the form, calling `getData()` will return that object
by default. If you want to return an associative array instead, you can pass the
`FormInterface::VALUES_AS_ARRAY` flag to the method.

```php
use Zend\Form\FormInterface;
$data = $form->getData(FormInterface::VALUES_AS_ARRAY);
```

Zend Framework ships several standard [hydrators](https://docs.zendframework.com/zend-hydrator/);
you can create custom hydrators by implementing `Zend\Hydrator\HydratorInterface`,
which looks like this:

``` sourceCode
namespace Zend\Hydrator;

interface HydratorInterface
{
    /** @return array */
    public function extract($object);
    public function hydrate(array $data, $object);
}
```

## Rendering

As noted previously, forms are meant to bridge the domain model and view layer.
We've discussed the domain model binding, but what about the view?

The form component ships a set of form-specific view helpers. These accept the
various form objects, and introspect them in order to generate markup.
Typically, they will inspect the attributes, but in special cases, they may look
at other properties and composed objects.

When preparing to render, you will generally want to call `prepare()`. This
method ensures that certain injections are done, and ensures that elements
nested in fieldsets and collections generate names in array notation (e.g.,
`scoped[array][notation]`).

The base view helpers used everywhere are `Form`, `FormElement`, `FormLabel`,
and `FormElementErrors`. Let's use them to display the contact form.

```php
<?php
// within a view script
$form = $this->form;
$form->prepare();

// Assuming the "contact/process" route exists...
$form->setAttribute('action', $this->url('contact/process'));

// Set the method attribute for the form
$form->setAttribute('method', 'post');

// Get the form label plugin
$formLabel = $this->plugin('formLabel');

// Render the opening tag
echo $this->form()->openTag($form);
?>
<div class="form_element">
<?php
    $name = $form->get('name');
    echo $formLabel->openTag() . $name->getOption('label');
    echo $this->formInput($name);
    echo $this->formElementErrors($name);
    echo $formLabel->closeTag();
?></div>

<div class="form_element">
<?php
    $subject = $form->get('subject');
    echo $formLabel->openTag() . $subject->getOption('label');
    echo $this->formInput($subject);
    echo $this->formElementErrors($subject);
    echo $formLabel->closeTag();
?></div>

<div class="form_element">
<?php
    $message = $form->get('message');
    echo $formLabel->openTag() . $message->getOption('label');
    echo $this->formTextarea($message);
    echo $this->formElementErrors($message);
    echo $formLabel->closeTag();
?></div>

<div class="form_element">
<?php
    $captcha = $form->get('captcha');
    echo $formLabel->openTag() . $captcha->getOption('label');
    echo $this->formCaptcha($captcha);
    echo $this->formElementErrors($captcha);
    echo $formLabel->closeTag();
?></div>

<?= $this->formElement($form->get('security')) ?>
<?= $this->formElement($form->get('send')) ?>

<?= $this->form()->closeTag() ?>
```

There are a few things to note about this. First, to prevent confusion in IDEs
and editors when syntax highlighting, we use helpers to both open and close the
form and label tags. Second, there's a lot of repetition happening here; we
could easily create a partial view script or a composite helper to reduce
boilerplate. Third, note that not all elements are created equal &mdash; the
CSRF and submit elements don't need labels or error messages. Finally, note that
the `FormElement` helper tries to do the right thing &mdash; it delegates actual
markup generation to other view helpers. However, it can only guess what
specific form helper to delegate to based on the list it has. If you introduce
new form view helpers, you'll need to extend the `FormElement` helper, or create
your own.

Following the example above, your view files can quickly become long and
repetitive to write. While we do not currently provide a single-line form view
helper (as this reduces the form customization), we do provide convenience
wrappers around emitting individual elements via the `FormRow` view helper, and
collections of elements (`Zend\Form\Element\Collection`, `Zend\Form\Fieldset`, or
`Zend\Form\Form`) via the `FormCollection` view helper (which, internally,
iterates the collection and calls `FormRow` for each element, recursively
following collections).

The `FormRow` view helper automatically renders a label (if present), the
element itself using the `FormElement` helper, as well as any errors that could
arise. Here is the previous form, rewritten to take advantage of this helper:

```php
<?php
// within a view script
$form = $this->form;
$form->prepare();

// Assuming the "contact/process" route exists...
$form->setAttribute('action', $this->url('contact/process'));

// Set the method attribute for the form
$form->setAttribute('method', 'post');

// Render the opening tag
echo $this->form()->openTag($form);
?>
<div class="form_element">
    <?= $this->formRow($form->get('name')) ?>
</div>

<div class="form_element">
    <?= $this->formRow($form->get('subject')) ?>
</div>

<div class="form_element">
    <?= $this->formRow($form->get('message')) ?>
</div>

<div class="form_element">
    <?= $this->formRow($form->get('captcha')) ?>
</div>

<?= $this->formElement($form->get('security')) ?>
<?= $this->formElement($form->get('send')) ?>

<?= $this->form()->closeTag() ?>
```

Note that `FormRow` helper automatically prepends the label. If you want it to
be rendered after the element itself, you can pass an optional parameter to the
`FormRow` view helper :

``` sourceCode
<div class="form_element">
    <?= $this->formRow($form->get('name'), 'append') ?>
</div>
```

As noted previously, the `FormCollection` view helper will iterate any
collection &mdash; including `Zend\Form\Element\Collection`, fieldsets, and
forms &mdash; emitting each element discovered using `FormRow`. `FormCollection`
*does not render fieldset or form tags*; you will be responsible for emitting
those yourself.

The above examples can now be rewritten again:

```php
<?php
// within a view script
$form = $this->form;
$form->prepare();

// Assuming the "contact/process" route exists...
$form->setAttribute('action', $this->url('contact/process'));

// Set the method attribute for the form
$form->setAttribute('method', 'post');

// Render the opening tag
echo $this->form()->openTag($form);
echo $this->formCollection($form);
echo $this->form()->closeTag();
```

Finally, the `Form` view helper can optionally accept a `Zend\Form\Form`
instance; if provided, it will prepare the form, iterate it, and render all
elements using either `FormRow` (for non-collection elements) or
`FormCollection` (for collections and fieldsets):

```php
<?php
// within a view script
$form = $this->form;

// Assuming the "contact/process" route exists...
$form->setAttribute('action', $this->url('contact/process'));

// Set the method attribute for the form
$form->setAttribute('method', 'post');

echo $this->form($form);
```

One important point to note about the last two examples: while they greatly
simplifies emitting the form, you also lose most customization opportunities.
The above, for example, will not include the `<div class="form_element"></div>`
wrappers from the previous examples! As such, you will generally want to use
this facility only when prototyping.

## Taking advantage of HTML5 input attributes

HTML5 brings a lot of exciting features, one of them being simplified client
form validations. zend-form provides elements corresponding to the various HTML5
elements, specifying the client-side attributes required by them. Additionally,
each implements `InputProviderInterface`, ensuring that your input filter will
have reasonable default validation and filtering rules that mimic the
client-side validations.

> ### Always validate server-side
>
> Although client validation is nice from a user experience point of view, it
> must be used in addition to server-side validation, as client validation can
> be easily bypassed.

## Validation Groups

Sometimes you want to validate only a subset of form elements. As an example,
let's say we're re-using our contact form over a web service; in this case, the
`Csrf`, `Captcha`, and submit button elements are not of interest, and shouldn't
be validated.

zend-form provides a proxy method to the underlying `InputFilter`'s
`setValidationGroup()` method, allowing us to perform this operation.

```php
$form->setValidationGroup('name', 'email', 'subject', 'message');
$form->setData($data);
if ($form->isValid()) {
    // Contains only the "name", "email", "subject", and "message" values
    $data = $form->getData();
}
```

If you later want to reset the form to validate all elements, pass the
`FormInterface::VALIDATE_ALL` flag to the `setValidationGroup()` method:

```php
use Zend\Form\FormInterface;
$form->setValidationGroup(FormInterface::VALIDATE_ALL);
```

When your form contains nested fieldsets, you can use an array notation to
validate only a subset of the fieldsets :

```php
$form->setValidationGroup(['profile' => [
    'firstname',
    'lastname',
] ]);

$form->setData($data);
if ($form->isValid()) {
    // Contains only the "firstname" and "lastname" values from the
    // "profile" fieldset
    $data = $form->getData();
}
```

## Using Annotations

Creating a complete form solution can often be tedious: you'll create a domain
model object, an input filter for validating it, a form object for providing a
representation for it, and potentially a hydrator for mapping the form elements
and fieldsets to the domain model. Wouldn't it be nice to have a central place
to define all of these?

Annotations allow us to solve this problem. You can define the following
behaviors with the shipped annotations in zend-form:

- `AllowEmpty`: mark an input as allowing an empty value. This annotation does
  not require a value.
- `Attributes`: specify the form, fieldset, or element attributes. This
  annotation requires an associative array of values, in a JSON object format:
  `@Attributes({"class":"zend_form","type":"text"})`.
- `ComposedObject`: specify another object with annotations to parse. Typically,
  this is used if a property references another object, which will then be added
  to your form as an additional fieldset.  Expects a string value indicating the
  class for the object being composed: `@ComposedObject("Namespace\Model\ComposedObject")`;
  or an array to compose a collection:
  `@ComposedObject({ "target_object":"Namespace\Model\ComposedCollection", "is_collection":"true", "options":{"count":2}})`;
  `target_object` is the element to compose, `is_collection` flags this as a
  collection, and `options` can take an array of options to pass into the
  collection.
- `ErrorMessage`: specify the error message to return for an element in the case
  of a failed validation. Expects a string value.
- `Exclude`: mark a property to exclude from the form or fieldset. This
  annotation does not require a value.
- `Filter`: provide a specification for a filter to use on a given element.
  Expects an associative array of values, with a "name" key pointing to a string
  filter name, and an "options" key pointing to an associative array of filter
  options for the constructor: `@Filter({"name": "Boolean", "options": {"casting":true}})`.
  This annotation may be specified multiple times.
- `Flags`: flags to pass to the fieldset or form composing an element or
  fieldset; these are usually used to specify the name or priority. The
  annotation expects an associative array: `@Flags({"priority": 100})`.
- `Hydrator`: specify the hydrator class to use for this given form or fieldset.
  A string value is expected.
- `InputFilter`: specify the input filter class to use for this given form or
  fieldset. A string value is expected.
- `Input`: specify the input class to use for this given element. A string value
  is expected.
- `Instance`: specify an object class instance to bind to the form or fieldset.
- `Name`: specify the name of the current element, fieldset, or form. A string
  value is expected.
- `Object`: specify an object class instance to bind to the form or fieldset.
  (Note: this is deprecated in 2.4.0; use `Instance` instead.)
- `Options`: options to pass to the fieldset or form that are used to inform
  behavior &mdash; things that are not attributes; e.g. labels, CAPTCHA adapters,
  etc. The annotation expects an associative array: `@Options({"label": "Username:"})`.
- `Required`: indicate whether an element is required. A boolean value is
  expected. By default, all elements are required, so this annotation is mainly
  present to allow disabling a requirement.
- `Type`: indicate the class to use for the current element, fieldset, or form.
  A string value is expected.
- `Validator`: provide a specification for a validator to use on a given
  element. Expects an associative array of values, with a "name" key pointing to
  a string validator name, and an "options" key pointing to an associative array
  of validator options for the constructor:
  `@Validator({"name": "StringLength", "options": {"min":3, "max": 25}})`.
  This annotation may be specified multiple times.

To use annotations, include them in your class and/or property docblocks.
Annotation names will be resolved according to the import statements in your
class; as such, you can make them as long or as short as you want depending on
what you import.

> ### doctrine/common dependency
>
> Form annotations require `doctrine\common`, which contains an annotation
> parsing engine. Install it using composer:
>
> ```bash
> $ composer require doctrine/common
> ```

Here's an example:

```php
use Zend\Form\Annotation;

/**
 * @Annotation\Name("user")
 * @Annotation\Hydrator("Zend\Hydrator\ObjectProperty")
 */
class User
{
    /**
     * @Annotation\Exclude()
     */
    public $id;

    /**
     * @Annotation\Filter({"name":"StringTrim"})
     * @Annotation\Validator({"name":"StringLength", "options":{"min":1, "max":25}})
     * @Annotation\Validator({"name":"Regex",
"options":{"pattern":"/^[a-zA-Z][a-zA-Z0-9_-]{0,24}$/"}})
     * @Annotation\Attributes({"type":"text"})
     * @Annotation\Options({"label":"Username:"})
     */
    public $username;

    /**
     * @Annotation\Type("Zend\Form\Element\Email")
     * @Annotation\Options({"label":"Your email address:"})
     */
    public $email;
}
```

The above will hint to the annotation builder to create a form with name "user",
which uses the hydrator `Zend\Hydrator\ObjectProperty`. That form will
have two elements, "username" and "email". The "username" element will have an
associated input that has a `StringTrim` filter, and two validators: a
`StringLength` validator indicating the username is between 1 and 25 characters,
and a `Regex` validator asserting it follows a specific accepted pattern. The
form element itself will have an attribute "type" with value "text" (a text
element), and a label "Username:". The "email" element will be of type
`Zend\Form\Element\Email`, and have the label "Your email address:".

To use the above, we need `Zend\Form\Annotation\AnnotationBuilder`:

```php
use Zend\Form\Annotation\AnnotationBuilder;

$builder = new AnnotationBuilder();
$form    = $builder->createForm(User::class);
```

At this point, you have a form with the appropriate hydrator attached, an input
filter with the appropriate inputs, and all elements.

> ### You're not done
>
> In all likelihood, you'll need to add some more elements to the form you
> construct. For example, you'll want a submit button, and likely a
> CSRF-protection element. We recommend creating a fieldset with common elements
> such as these that you can then attach to the form you build via annotations.
