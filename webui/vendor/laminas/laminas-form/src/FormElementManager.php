<?php

/**
 * @see       https://github.com/laminas/laminas-form for the canonical source repository
 * @copyright https://github.com/laminas/laminas-form/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-form/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Form;

use Laminas\ServiceManager\AbstractPluginManager;
use Laminas\ServiceManager\ConfigInterface;
use Laminas\ServiceManager\Exception\ServiceNotCreatedException;
use Laminas\ServiceManager\ServiceLocatorInterface;
use Laminas\Stdlib\InitializableInterface;

/**
 * Plugin manager implementation for form elements.
 *
 * Enforces that elements retrieved are instances of ElementInterface.
 */
class FormElementManager extends AbstractPluginManager
{
    /**
     * Default set of helpers
     *
     * @var array
     */
    protected $invokableClasses = [
        'button'        => 'Laminas\Form\Element\Button',
        'captcha'       => 'Laminas\Form\Element\Captcha',
        'checkbox'      => 'Laminas\Form\Element\Checkbox',
        'collection'    => 'Laminas\Form\Element\Collection',
        'color'         => 'Laminas\Form\Element\Color',
        'csrf'          => 'Laminas\Form\Element\Csrf',
        'date'          => 'Laminas\Form\Element\Date',
        'dateselect'    => 'Laminas\Form\Element\DateSelect',
        'datetime'      => 'Laminas\Form\Element\DateTime',
        'datetimelocal' => 'Laminas\Form\Element\DateTimeLocal',
        'datetimeselect' => 'Laminas\Form\Element\DateTimeSelect',
        'element'       => 'Laminas\Form\Element',
        'email'         => 'Laminas\Form\Element\Email',
        'fieldset'      => 'Laminas\Form\Fieldset',
        'file'          => 'Laminas\Form\Element\File',
        'form'          => 'Laminas\Form\Form',
        'hidden'        => 'Laminas\Form\Element\Hidden',
        'image'         => 'Laminas\Form\Element\Image',
        'month'         => 'Laminas\Form\Element\Month',
        'monthselect'   => 'Laminas\Form\Element\MonthSelect',
        'multicheckbox' => 'Laminas\Form\Element\MultiCheckbox',
        'number'        => 'Laminas\Form\Element\Number',
        'password'      => 'Laminas\Form\Element\Password',
        'radio'         => 'Laminas\Form\Element\Radio',
        'range'         => 'Laminas\Form\Element\Range',
        'select'        => 'Laminas\Form\Element\Select',
        'submit'        => 'Laminas\Form\Element\Submit',
        'text'          => 'Laminas\Form\Element\Text',
        'textarea'      => 'Laminas\Form\Element\Textarea',
        'time'          => 'Laminas\Form\Element\Time',
        'url'           => 'Laminas\Form\Element\Url',
        'week'          => 'Laminas\Form\Element\Week',
    ];

    /**
     * Don't share form elements by default
     *
     * @var bool
     */
    protected $shareByDefault = false;

    /**
     * @param ConfigInterface $configuration
     */
    public function __construct(ConfigInterface $configuration = null)
    {
        parent::__construct($configuration);

        $this->addInitializer([$this, 'injectFactory']);
        $this->addInitializer([$this, 'callElementInit'], false);
    }

    /**
     * Inject the factory to any element that implements FormFactoryAwareInterface
     *
     * @param $element
     */
    public function injectFactory($element)
    {
        if ($element instanceof FormFactoryAwareInterface) {
            $factory = $element->getFormFactory();
            $factory->setFormElementManager($this);

            if ($this->serviceLocator instanceof ServiceLocatorInterface
                && $this->serviceLocator->has('InputFilterManager')
            ) {
                $inputFilters = $this->serviceLocator->get('InputFilterManager');
                $factory->getInputFilterFactory()->setInputFilterManager($inputFilters);
            }
        }
    }

    /**
     * Call init() on any element that implements InitializableInterface
     *
     * @internal param $element
     */
    public function callElementInit($element)
    {
        if ($element instanceof InitializableInterface) {
            $element->init();
        }
    }

    /**
     * Validate the plugin
     *
     * Checks that the element is an instance of ElementInterface
     *
     * @param  mixed $plugin
     * @throws Exception\InvalidElementException
     * @return void
     */
    public function validatePlugin($plugin)
    {
        if ($plugin instanceof ElementInterface) {
            return; // we're okay
        }

        throw new Exception\InvalidElementException(sprintf(
            'Plugin of type %s is invalid; must implement Laminas\Form\ElementInterface',
            (is_object($plugin) ? get_class($plugin) : gettype($plugin))
        ));
    }

    /**
     * Retrieve a service from the manager by name
     *
     * Allows passing an array of options to use when creating the instance.
     * createFromInvokable() will use these and pass them to the instance
     * constructor if not null and a non-empty array.
     *
     * @param  string $name
     * @param  string|array $options
     * @param  bool $usePeeringServiceManagers
     * @return object
     */
    public function get($name, $options = [], $usePeeringServiceManagers = true)
    {
        if (is_string($options)) {
            $options = ['name' => $options];
        }
        return parent::get($name, $options, $usePeeringServiceManagers);
    }

    /**
     * Attempt to create an instance via an invokable class
     *
     * Overrides parent implementation by passing $creationOptions to the
     * constructor, if non-null.
     *
     * @param  string $canonicalName
     * @param  string $requestedName
     * @return null|\stdClass
     * @throws ServiceNotCreatedException If resolved class does not exist
     */
    protected function createFromInvokable($canonicalName, $requestedName)
    {
        $invokable = $this->invokableClasses[$canonicalName];

        if (null === $this->creationOptions
            || (is_array($this->creationOptions) && empty($this->creationOptions))
        ) {
            $instance = new $invokable();
        } else {
            if (isset($this->creationOptions['name'])) {
                $name = $this->creationOptions['name'];
            } else {
                $name = $requestedName;
            }

            if (isset($this->creationOptions['options'])) {
                $options = $this->creationOptions['options'];
            } else {
                $options = $this->creationOptions;
            }

            $instance = new $invokable($name, $options);
        }

        return $instance;
    }
}
