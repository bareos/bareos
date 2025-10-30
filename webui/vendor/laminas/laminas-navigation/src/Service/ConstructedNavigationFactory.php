<?php

/**
 * @see       https://github.com/laminas/laminas-navigation for the canonical source repository
 * @copyright https://github.com/laminas/laminas-navigation/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-navigation/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Navigation\Service;

use Interop\Container\ContainerInterface;

/**
 * Constructed factory to set pages during construction.
 */
class ConstructedNavigationFactory extends AbstractNavigationFactory
{
    /**
     * @var string|\Laminas\Config\Config|array
     */
    protected $config;

    /**
     * @param string|\Laminas\Config\Config|array $config
     */
    public function __construct($config)
    {
        $this->config = $config;
    }

    /**
     * @param ContainerInterface $container
     * @return array|null|\Laminas\Config\Config
     */
    public function getPages(ContainerInterface $container)
    {
        if (null === $this->pages) {
            $this->pages = $this->preparePages($container, $this->getPagesFromConfig($this->config));
        }
        return $this->pages;
    }

    /**
     * @return string
     */
    public function getName()
    {
        return 'constructed';
    }
}
