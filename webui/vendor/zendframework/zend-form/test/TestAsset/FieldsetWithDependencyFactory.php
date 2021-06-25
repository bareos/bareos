<?php
/**
 * @link      http://github.com/zendframework/zend-form for the canonical source repository
 * @copyright Copyright (c) 2005-2016 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Form\TestAsset;

use Interop\Container\ContainerInterface;
use Zend\ServiceManager\FactoryInterface;
use Zend\ServiceManager\ServiceLocatorInterface;

class FieldsetWithDependencyFactory implements FactoryInterface
{
    private $creationOptions = [];

    public function __invoke(ContainerInterface $container, $name, array $options = null)
    {
        $options = $options ?: [];

        $name = null;
        if (isset($options['name'])) {
            $name = $options['name'];
            unset($options['name']);
        }

        $form = new FieldsetWithDependency($name, $options);
        $form->setDependency(new InputFilter());

        return $form;
    }

    public function createService(ServiceLocatorInterface $container)
    {
        return $this($container, CustomCreatedForm::class, $this->creationOptions);
    }

    public function setCreationOptions(array $options)
    {
        $this->creationOptions = $options;
    }
}
