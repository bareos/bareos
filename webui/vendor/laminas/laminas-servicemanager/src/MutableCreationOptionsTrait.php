<?php

/**
 * @see       https://github.com/laminas/laminas-servicemanager for the canonical source repository
 * @copyright https://github.com/laminas/laminas-servicemanager/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-servicemanager/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\ServiceManager;

/**
 * Trait for MutableCreationOptions Factories
 */
trait MutableCreationOptionsTrait
{
    /**
     * @var array
     */
    protected $creationOptions = [];

    /**
     * Set creation options
     *
     * @param array $creationOptions
     * @return void
     */
    public function setCreationOptions(array $creationOptions)
    {
        $this->creationOptions = $creationOptions;
    }

    /**
     * Get creation options
     *
     * @return array
     */
    public function getCreationOptions()
    {
        return $this->creationOptions;
    }
}
