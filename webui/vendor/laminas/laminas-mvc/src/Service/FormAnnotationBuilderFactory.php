<?php

/**
 * @see       https://github.com/laminas/laminas-mvc for the canonical source repository
 * @copyright https://github.com/laminas/laminas-mvc/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-mvc/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Mvc\Service;

use Laminas\EventManager\ListenerAggregateInterface;
use Laminas\Form\Annotation\AnnotationBuilder;
use Laminas\ServiceManager\Exception\RuntimeException;
use Laminas\ServiceManager\FactoryInterface;
use Laminas\ServiceManager\ServiceLocatorInterface;

class FormAnnotationBuilderFactory implements FactoryInterface
{
    /**
     * Create service
     *
     * @param ServiceLocatorInterface $serviceLocator
     * @throws \Laminas\ServiceManager\Exception\RuntimeException
     * @return mixed
     */
    public function createService(ServiceLocatorInterface $serviceLocator)
    {
        //setup a form factory which can use custom form elements
        $annotationBuilder = new AnnotationBuilder();
        $formElementManager = $serviceLocator->get('FormElementManager');
        $formElementManager->injectFactory($annotationBuilder, $serviceLocator);

        $config = $serviceLocator->get('Config');
        if (isset($config['form_annotation_builder'])) {
            $config = $config['form_annotation_builder'];

            if (isset($config['annotations'])) {
                foreach ((array) $config['annotations'] as $fullyQualifiedClassName) {
                    $annotationBuilder->getAnnotationParser()->registerAnnotation($fullyQualifiedClassName);
                }
            }

            if (isset($config['listeners'])) {
                foreach ((array) $config['listeners'] as $listenerName) {
                    $listener = $serviceLocator->get($listenerName);
                    if (!($listener instanceof ListenerAggregateInterface)) {
                        throw new RuntimeException(sprintf('Invalid event listener (%s) provided', $listenerName));
                    }
                    $listener->attach($annotationBuilder->getEventManager());
                }
            }

            if (isset($config['preserve_defined_order'])) {
                $annotationBuilder->setPreserveDefinedOrder($config['preserve_defined_order']);
            }
        }

        return $annotationBuilder;
    }
}
