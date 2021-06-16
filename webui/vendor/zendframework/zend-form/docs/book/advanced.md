# Advanced use cases

Beginning with version 2.1, forms elements can be registered using a
designated plugin manager, in the same way that view helpers, controller
plugins, and filters are registered. This new feature has a number of benefits,
especially when you need to handle complex dependencies in forms/fieldsets.

## Short names

The first advantage of pulling form elements from the service manager is that
now you can use short names to create new elements through the factory.
Therefore, this code:

```php
$form->add([
    'type' => Element\Email::class,
    'name' => 'email',
]);
```

can now be replaced by:

```php
$form->add([
     'type' => 'Email',
     'name' => 'email'
]);
```

Each element provided out-of-the-box by zend-form supports this natively.

> ### Use the ::class constant
>
> While using aliases leads to compact code, they're also can more easily result
> in typographic mistakes. We recommend using the `::class` constant in most
> situations, as these can be more easily scanned with static analysis tools for
> correctness.

## Creating custom elements

zend-form also supports custom form elements.

To create a custom form element, make it extend the `Zend\Form\Element` class,
or, if you have a more specific dependency, extend one of the classes in the
`Zend\Form\Element` namespace.

In the following, we will demonstrate creating a custom `Phone` element for
entering phone numbers. It will extend `Zend\Form\Element` class and provide
some default input rules.

Our custom phone element could look something like this:

```php
namespace Application\Form\Element;

use Zend\Filter;
use Zend\Form\Element;
use Zend\InputFilter\InputProviderInterface;
use Zend\Validator\Regex as RegexValidator;

class Phone extends Element implements InputProviderInterface
{
    /**
     * @var ValidatorInterface
     */
    protected $validator;

    /**
    * Get a validator if none has been set.
    *
    * @return ValidatorInterface
    */
    public function getValidator()
    {
        if (null === $this->validator) {
            $validator = new RegexValidator('/^\+?\d{11,12}$/');
            $validator->setMessage(
                'Please enter 11 or 12 digits only!',
                RegexValidator::NOT_MATCH
            );

            $this->validator = $validator;
        }

        return $this->validator;
    }

    /**
     * Sets the validator to use for this element
     *
     * @param  ValidatorInterface $validator
     * @return self
     */
    public function setValidator(ValidatorInterface $validator)
    {
        $this->validator = $validator;
        return $this;
    }

    /**
     * Provide default input rules for this element
     *
     * Attaches a phone number validator.
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
            ],
            'validators' => [
                $this->getValidator(),
            ],
        ];
    }
}
```

By implementing `Zend\InputFilter\InputProviderInterface` interface, we are
hinting to our form object that this element provides some default input rules
for filtering and/or validating values. In this example, the default input
specification provides a `Zend\Filter\StringTrim` filter and a
`Zend\Validator\Regex` validator that validates that the value optionally has a
`+` sign at the beginning, and is followed by 11 or 12 digits.

To use the new element in our forms, we can specify it by its fully qualified
class name (FQCN):

```php
use Application\Form\Element\Phone;
use Zend\Form\Form;

$form = Form();
$form->add(array(
    'name' => 'phone',
    'type' => Phone::class,
));
```

Or, if you are extending `Zend\Form\Form`:

```php
namespace Application\Form;

use Zend\Form\Form;

class MyForm extends Form
{
    public function __construct($name = null)
    {
        parent::__construct($name);

        $this->add([
            'name' => 'phone',
            'type' => Element\Phone::class,
        ]);
    }
}
```

If you don't want to use the custom element's FQCN, but rather a short name,
add an entry for it to `Zend\Form\FormElementManager`. You can do this by adding
an entry under the `form_elements` configuration, or within your `Module` class
via a `getFormElementConfig()` method.

Configuration via a config file (e.g., `module.config.php`) file looks like the
following:

```php
use Zend\ServiceManager\Factory\InvokableFactory;

return [
    'form_elements' => [
        'aliases' => [
            'phone' => Application\Form\Element\Phone::class,
        ],
        'factories' => [
            Application\Form\Element\Phone::class => InvokableFactory::class,
        ],
    ],
];
```

The following demonstrates using your `Module` class:

```php
namespace Application;

use Zend\ModuleManager\Feature\FormElementProviderInterface;
use Zend\ServiceManager\Factory\InvokableFactory;

class Module implements FormElementProviderInterface
{
    public function getFormElementConfig()
    {
        return [
            'aliases' => [
                'phone' => Form\Element\Phone::class,
            ],
            'factories' => [
                Form\Element\Phone::class => InvokableFactory::class,
            ],
        ];
    }
}
```

If needed, you can define a custom factory for handling dependencies.

**And now comes the first catch.**

If you are creating your form class by extending `Zend\Form\Form`, you *must
not* add the custom element in the constructor (as we have done in the previous
example where we used the custom element's FQCN), but rather in the `init()`
method:

```php
namespace Application\Form;

use Zend\Form\Form;

class MyForm extends Form
{
    public function init()
    {
        $this->add([
            'name' => 'phone',
            'type' => 'phone',
        ]);
    }
}
```

**The second catch** is that you *must not* directly instantiate your form
class, but rather get an instance of it through `Zend\Form\FormElementManager`:

```php
namespace Application\Controller;

use Application\Form\MyForm;
use Zend\Mvc\Controller\AbstractActionController;

class IndexController extends AbstractActionController
{
    private $form;

    public function __construct(MyForm $form)
    {
        $this->form = $form;
    }

    public function indexAction()
    {
        return array('form' => $this->form);
    }
}
```

This now requires a factory to inject the form instance:

```php
namespace Application\Controller;

use Interop\Container\ContainerInterface;
use Application\Form\MyForm;

class IndexControllerFactory
{
    public function __invoke(ContainerInterface $container)
    {
        $formManager = $container->get('FormElementManager');
        return new IndexController($formManager->get(MyForm::class));
    }
}
```

Which in turn requires that you map the controller to the factory:

```php
// In module.config.php
return [
    /* ... */
    'controllers' => [
        'factories' => [
            Application\Controller\IndexController::class => Application\Controller\IndexControllerFactory::class,
        ],
    ],
];
```

The biggest gain of this is that you can easily override any built-in form
elements if they do not fit your needs. For instance, if you want to create your
own `Email` element instead of the standard one, create your custom element, and
add it to the form element config with the same key as the element you want to
replace:

```php
namespace Application;

use Zend\Form\Element\Email;
use Zend\ModuleManager\Feature\FormElementProviderInterface;
use Zend\ServiceManager\Factory\InvokableFactory;

class Module implements FormElementProviderInterface
{
    public function getFormElementConfig()
    {
        return [
            'aliases' => [
                'email' => Form\Element\MyEmail::class,
                'Email' => Form\Element\MyEmail::class,
            ],
            'factories' => [
                Form\Element\MyEmail::class => InvokableFactory::class,
            ],
        ];
    }
}
```

Now whenever you create an element with a `type` of 'email',
it will create the custom element instead of the built-in one.

> ### Use the original?
>
> If you want to be able to use both the built-in one and your own one, you can
> still provide the FQCN of the element, e.g. `Zend\Form\Element\Email`.

In summary, to create your own form elements (or even reusable fieldsets!) and
be able to use them in your form, you need to:

1. Create your element (like you did before).
2. Add it to the form element manager either via the `form_elements`
   configuration in your module, or by defining a `getFormElementConfig()` in
   your `Module` class.
3. Make sure the custom form element is not added in the form's constructor,
   but rather in its `init()` method, or after getting an instance of the form.
4. Retrieve your form through the form element manager instead of directly
   instantiating it, and inject it in your controller.

## Handling dependencies

Dependency management can be complex. For instance, a very frequent use case is
a form that creates a fieldset, but itself need access to the database to
populate a `Select` element. Retrieving forms from the `FormElementManager`
solves this issue, as factories it invokes have access to the application
service container, and can use it to provide dependencies.

For instance, let's say that a form create a fieldset called `AlbumFieldset`:

```php
namespace Application\Form;

use Zend\Form\Form;

class CreateAlbum extends Form
{
    public function init()
    {
        $this->add([
            'name' => 'album',
            'type' => AlbumFieldset::class,
        ]);
    }
}
```

Let's now create the `AlbumFieldset`, and have it depend on an `AlbumTable`
object that allows us to fetch albums from the database.

```php
namespace Application\Form;

use Album\Model\AlbumTable;
use Zend\Form\Fieldset;

class AlbumFieldset extends Fieldset
{
    public function __construct(AlbumTable $albumTable)
    {
        // Add any elements that need to fetch data from database
        // using the album table !
    }
}
```

To enable this, we'll create a factory for our `AlbumFieldset` as follows:

```php
namespace Application\Form;

use Album\Model\AlbumTable;
use Interop\Container\ContainerInterface;
use Zend\ServiceManager\FactoryInterface;
use Zend\ServiceManager\ServiceLocatorInterface;

class AlbumFieldsetFactory implements FactoryInterface
{
    public function __invoke(ContainerInterface $container, $name, array $options = null)
    {
        return new AlbumFieldset($container->get(AlbumTable::class));
    }

    public function createService(ServiceLocatorInterface $formManager)
    {
        return $this(
            $formManager->getServiceLocator() ?: $formManager,
            AlbumFieldset::class
        );
    }
}
```

> ### Compatibility
>
> The above factory was written to work with both the v2 and v3 releases of
> zend-servicemanager. If you know you will only be using v3, you can remove the
> `createService()` implementation.

You can now map the fieldset to the factory in your configuration:

```php
// In module.config.php:
return [
    'form_elements' => [
        'factories' => [
            Application\Form\AlbumFieldset::class => Application\Form\AlbumFieldsetFactory::class,
        ],
    ],
];
```

Inject your form into your controller, per the example in the previous section.

As a reminder, to use your fieldset in a view, you need to use the
`formCollection` helper:

```php
echo $this->form()->openTag($form);
echo $this->formCollection($form->get('album'));
echo $this->form()->closeTag();
```

## Initialization

As noted in previous sections, and in the chapter on elements, we recommend
defining an `init()` method for initializing your elements, fieldsets, and
forms. Where does this come from, and when exactly is it invoked in the object
lifecycle?

The method is defined in `Zend\Stdlib\InitializableInterface`, which
`Zend\Form\Element` implements. It is *not*, however, automatically invoked on
instantiation!

Within zend-form, the `FormElementManager` defines an
[initializer](http://docs.zendframework.com/zend-servicemanager/configuring-the-service-manager/#initializers)
that is pushed to the bottom of the initializer stack, making it the last
initializer invoked. This initializer checks if the instance created implements
`InitializableInterface`, and, if so, calls its `init()` method.

This approach ensures that dependencies are fully injected prior to any methods
you call from your `init()` method. As a result, when pulling items from the
`FormElementManager`, you can be assured that all factories are correctly setup
and populated, and shared across all specifications you provide.
