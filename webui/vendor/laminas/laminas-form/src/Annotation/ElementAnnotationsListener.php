<?php

/**
 * @see       https://github.com/laminas/laminas-form for the canonical source repository
 * @copyright https://github.com/laminas/laminas-form/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-form/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Form\Annotation;

use Laminas\EventManager\EventManagerInterface;
use Laminas\Stdlib\ArrayObject;

/**
 * Default listeners for element annotations
 *
 * Defines and attaches a set of default listeners for element annotations
 * (which are defined on object properties). These include:
 *
 * - AllowEmpty
 * - Attributes
 * - ErrorMessage
 * - Filter
 * - Flags
 * - Input
 * - Hydrator
 * - Object and Instance (the latter is preferred starting in 2.4)
 * - Required
 * - Type
 * - Validator
 *
 * See the individual annotation classes for more details. The handlers registered
 * work with the annotation values, as well as the element and input specification
 * passed in the event object.
 */
class ElementAnnotationsListener extends AbstractAnnotationsListener
{
    /**
     * {@inheritDoc}
     */
    public function attach(EventManagerInterface $events)
    {
        $this->listeners[] = $events->attach('configureElement', [$this, 'handleAllowEmptyAnnotation']);
        $this->listeners[] = $events->attach('configureElement', [$this, 'handleAttributesAnnotation']);
        $this->listeners[] = $events->attach('configureElement', [$this, 'handleComposedObjectAnnotation']);
        $this->listeners[] = $events->attach('configureElement', [$this, 'handleContinueIfEmptyAnnotation']);
        $this->listeners[] = $events->attach('configureElement', [$this, 'handleErrorMessageAnnotation']);
        $this->listeners[] = $events->attach('configureElement', [$this, 'handleFilterAnnotation']);
        $this->listeners[] = $events->attach('configureElement', [$this, 'handleFlagsAnnotation']);
        $this->listeners[] = $events->attach('configureElement', [$this, 'handleHydratorAnnotation']);
        $this->listeners[] = $events->attach('configureElement', [$this, 'handleInputAnnotation']);
        $this->listeners[] = $events->attach('configureElement', [$this, 'handleObjectAnnotation']);
        $this->listeners[] = $events->attach('configureElement', [$this, 'handleOptionsAnnotation']);
        $this->listeners[] = $events->attach('configureElement', [$this, 'handleRequiredAnnotation']);
        $this->listeners[] = $events->attach('configureElement', [$this, 'handleTypeAnnotation']);
        $this->listeners[] = $events->attach('configureElement', [$this, 'handleValidatorAnnotation']);

        $this->listeners[] = $events->attach('discoverName', [$this, 'handleNameAnnotation']);
        $this->listeners[] = $events->attach('discoverName', [$this, 'discoverFallbackName']);

        $this->listeners[] = $events->attach('checkForExclude', [$this, 'handleExcludeAnnotation']);
    }

    /**
     * Handle the AllowEmpty annotation
     *
     * Sets the allow_empty flag on the input specification array.
     *
     * @param  \Laminas\EventManager\EventInterface $e
     * @return void
     */
    public function handleAllowEmptyAnnotation($e)
    {
        $annotation = $e->getParam('annotation');
        if (!$annotation instanceof AllowEmpty) {
            return;
        }

        $inputSpec = $e->getParam('inputSpec');
        $inputSpec['allow_empty'] = true;
    }

    /**
     * Handle the Attributes annotation
     *
     * Sets the attributes array of the element specification.
     *
     * @param  \Laminas\EventManager\EventInterface $e
     * @return void
     */
    public function handleAttributesAnnotation($e)
    {
        $annotation = $e->getParam('annotation');
        if (!$annotation instanceof Attributes) {
            return;
        }

        $elementSpec = $e->getParam('elementSpec');
        if (isset($elementSpec['spec']['attributes'])) {
            $elementSpec['spec']['attributes'] = array_merge(
                $elementSpec['spec']['attributes'],
                $annotation->getAttributes()
            );
            return;
        }

        $elementSpec['spec']['attributes'] = $annotation->getAttributes();
    }

    /**
     * Allow creating fieldsets from composed entity properties
     *
     * @param  \Laminas\EventManager\EventInterface $e
     * @return void
     */
    public function handleComposedObjectAnnotation($e)
    {
        $annotation = $e->getParam('annotation');
        if (!$annotation instanceof ComposedObject) {
            return;
        }

        $class             = $annotation->getComposedObject();
        $annotationManager = $e->getTarget();
        $specification     = $annotationManager->getFormSpecification($class);

        $name        = $e->getParam('name');
        $elementSpec = $e->getParam('elementSpec');

        if ($annotation->isCollection()) {
            // Compose specification as a fieldset into parent form/fieldset
            if (!isset($specification['type'])) {
                //use input filter provider fieldset so we can compose the input filter into the fieldset
                //it is assumed that if someone uses a custom fieldset, they will take care of the input
                //filtering themselves or consume the input_filter_spec option.
                $specification['type'] = 'Laminas\Form\InputFilterProviderFieldset';
            }

            $inputFilter = $specification['input_filter'];
            if (!isset($inputFilter['type'])) {
                $inputFilter['type'] = 'Laminas\InputFilter\InputFilter';
            }
            unset($specification['input_filter']);

            $elementSpec['spec']['type'] = 'Laminas\Form\Element\Collection';
            $elementSpec['spec']['name'] = $name;
            $elementSpec['spec']['options'] = new ArrayObject($this->mergeOptions($elementSpec, $annotation));
            $elementSpec['spec']['options']['target_element'] = $specification;
            $elementSpec['spec']['options']['target_element']['options']['input_filter_spec'] = $inputFilter;

            if (isset($specification['hydrator'])) {
                $elementSpec['spec']['hydrator'] = $specification['hydrator'];
            }
        } else {
            // Compose input filter into parent input filter
            $inputFilter = $specification['input_filter'];
            if (!isset($inputFilter['type'])) {
                $inputFilter['type'] = 'Laminas\InputFilter\InputFilter';
            }
            $e->setParam('inputSpec', $inputFilter);
            unset($specification['input_filter']);

            // Compose specification as a fieldset into parent form/fieldset
            if (!isset($specification['type'])) {
                $specification['type'] = 'Laminas\Form\Fieldset';
            }

            if (isset($elementSpec['spec']['options'])) {
                $specification['options'] = isset($specification['options']) ? $specification['options'] : [];
                $specification['options'] = array_merge($elementSpec['spec']['options'], $specification['options']);
            }

            // Add element spec:
            $elementSpec['spec'] = $specification;
            $elementSpec['spec']['name'] = $name;
            $elementSpec['spec']['options'] = new ArrayObject($this->mergeOptions($elementSpec, $annotation));
        }
    }

    /**
     * Handle the ContinueIfEmpty annotation
     *
     * Sets the continue_if_empty flag on the input specification array.
     *
     * @param  \Laminas\EventManager\EventInterface $e
     * @return void
     */
    public function handleContinueIfEmptyAnnotation($e)
    {
        $annotation = $e->getParam('annotation');
        if (!$annotation instanceof ContinueIfEmpty) {
            return;
        }

        $inputSpec = $e->getParam('inputSpec');
        $inputSpec['continue_if_empty'] = true;
    }

    /**
     * Handle the ErrorMessage annotation
     *
     * Sets the error_message of the input specification.
     *
     * @param  \Laminas\EventManager\EventInterface $e
     * @return void
     */
    public function handleErrorMessageAnnotation($e)
    {
        $annotation = $e->getParam('annotation');
        if (!$annotation instanceof ErrorMessage) {
            return;
        }

        $inputSpec = $e->getParam('inputSpec');
        $inputSpec['error_message'] = $annotation->getMessage();
    }

    /**
     * Determine if the element has been marked to exclude from the definition
     *
     * @param  \Laminas\EventManager\EventInterface $e
     * @return bool
     */
    public function handleExcludeAnnotation($e)
    {
        $annotations = $e->getParam('annotations');
        if ($annotations->hasAnnotation('Laminas\Form\Annotation\Exclude')) {
            return true;
        }
        return false;
    }

    /**
     * Handle the Filter annotation
     *
     * Adds a filter to the filter chain specification for the input.
     *
     * @param  \Laminas\EventManager\EventInterface $e
     * @return void
     */
    public function handleFilterAnnotation($e)
    {
        $annotation = $e->getParam('annotation');
        if (!$annotation instanceof Filter) {
            return;
        }

        $inputSpec = $e->getParam('inputSpec');
        if (!isset($inputSpec['filters'])) {
            $inputSpec['filters'] = [];
        }
        $inputSpec['filters'][] = $annotation->getFilter();
    }

    /**
     * Handle the Flags annotation
     *
     * Sets the element flags in the specification (used typically for setting
     * priority).
     *
     * @param  \Laminas\EventManager\EventInterface $e
     * @return void
     */
    public function handleFlagsAnnotation($e)
    {
        $annotation = $e->getParam('annotation');
        if (!$annotation instanceof Flags) {
            return;
        }

        $elementSpec = $e->getParam('elementSpec');
        $elementSpec['flags'] = $annotation->getFlags();
    }

    /**
     * Handle the Hydrator annotation
     *
     * Sets the hydrator class to use in the fieldset specification.
     *
     * @param  \Laminas\EventManager\EventInterface $e
     * @return void
     */
    public function handleHydratorAnnotation($e)
    {
        $annotation = $e->getParam('annotation');
        if (!$annotation instanceof Hydrator) {
            return;
        }

        $elementSpec = $e->getParam('elementSpec');
        $elementSpec['spec']['hydrator'] = $annotation->getHydrator();
    }

    /**
     * Handle the Input annotation
     *
     * Sets the filter specification for the current element to the specified
     * input class name.
     *
     * @param  \Laminas\EventManager\EventInterface $e
     * @return void
     */
    public function handleInputAnnotation($e)
    {
        $annotation = $e->getParam('annotation');
        if (!$annotation instanceof Input) {
            return;
        }

        $inputSpec = $e->getParam('inputSpec');
        $inputSpec['type'] = $annotation->getInput();
    }

    /**
     * Handle the Object and Instance annotations
     *
     * Sets the object to bind to the form or fieldset
     *
     * @param  \Laminas\EventManager\EventInterface $e
     * @return void
     */
    public function handleObjectAnnotation($e)
    {
        $annotation = $e->getParam('annotation');

        // Only need to typehint on Instance, as Object extends it
        if (! $annotation instanceof Instance) {
            return;
        }

        $elementSpec = $e->getParam('elementSpec');
        $elementSpec['spec']['object'] = $annotation->getObject();
    }

    /**
     * Handle the Options annotation
     *
     * Sets the element options in the specification.
     *
     * @param  \Laminas\EventManager\EventInterface $e
     * @return void
     */
    public function handleOptionsAnnotation($e)
    {
        $annotation = $e->getParam('annotation');
        if (!$annotation instanceof Options) {
            return;
        }

        $elementSpec = $e->getParam('elementSpec');
        $elementSpec['spec']['options'] = $this->mergeOptions($elementSpec, $annotation);
    }

    /**
     * Handle the Required annotation
     *
     * Sets the required flag on the input based on the annotation value.
     *
     * @param  \Laminas\EventManager\EventInterface $e
     * @return void
     */
    public function handleRequiredAnnotation($e)
    {
        $annotation = $e->getParam('annotation');
        if (!$annotation instanceof Required) {
            return;
        }

        $required  = (bool) $annotation->getRequired();
        $inputSpec = $e->getParam('inputSpec');
        $inputSpec['required'] = $required;

        if ($required) {
            $elementSpec = $e->getParam('elementSpec');
            if (!isset($elementSpec['spec']['attributes'])) {
                $elementSpec['spec']['attributes'] = [];
            }

            $elementSpec['spec']['attributes']['required'] = 'required';
        }
    }

    /**
     * Handle the Type annotation
     *
     * Sets the element class type to use in the element specification.
     *
     * @param  \Laminas\EventManager\EventInterface $e
     * @return void
     */
    public function handleTypeAnnotation($e)
    {
        $annotation = $e->getParam('annotation');
        if (!$annotation instanceof Type) {
            return;
        }

        $elementSpec = $e->getParam('elementSpec');
        $elementSpec['spec']['type'] = $annotation->getType();
    }

    /**
     * Handle the Validator annotation
     *
     * Adds a validator to the validator chain of the input specification.
     *
     * @param  \Laminas\EventManager\EventInterface $e
     * @return void
     */
    public function handleValidatorAnnotation($e)
    {
        $annotation = $e->getParam('annotation');
        if (!$annotation instanceof Validator) {
            return;
        }

        $inputSpec = $e->getParam('inputSpec');
        if (!isset($inputSpec['validators'])) {
            $inputSpec['validators'] = [];
        }
        $inputSpec['validators'][] = $annotation->getValidator();
    }

    /**
     * @param array|\ArrayAccess     $elementSpec
     * @param ComposedObject|Options $annotation
     *
     * @return array
     */
    private function mergeOptions($elementSpec, $annotation)
    {
        if (isset($elementSpec['spec']['options'])) {
            if (is_array($elementSpec['spec']['options'])) {
                return array_merge($elementSpec['spec']['options'], $annotation->getOptions());
            }

            if ($elementSpec['spec']['options'] instanceof ArrayObject) {
                return array_merge($elementSpec['spec']['options']->getArrayCopy(), $annotation->getOptions());
            }
        }

        return $annotation->getOptions();
    }
}
