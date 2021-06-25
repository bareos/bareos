<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2015 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Form\TestAsset;

use Interop\Container\ContainerInterface;
use Zend\ServiceManager\FactoryInterface;
use Zend\ServiceManager\ServiceLocatorInterface;
use DateTime;

class CustomCreatedFormFactory implements FactoryInterface
{
    private $creationOptions = [];

    public function __invoke(ContainerInterface $container, $name, array $options = null)
    {
        $options = $options ?: [];
        $creationString = 'now';

        if (isset($options['created'])) {
            $creationString = $options['created'];
            unset($options['created']);
        }

        $created = new DateTime($creationString);

        $name = null;
        if (isset($options['name'])) {
            $name = $options['name'];
            unset($options['name']);
        }

        $form = new CustomCreatedForm($created, $name, $options);
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
