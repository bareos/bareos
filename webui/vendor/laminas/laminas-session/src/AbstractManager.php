<?php

/**
 * @see       https://github.com/laminas/laminas-session for the canonical source repository
 * @copyright https://github.com/laminas/laminas-session/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-session/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Session;

use Laminas\Session\Config\ConfigInterface as Config;
use Laminas\Session\ManagerInterface as Manager;
use Laminas\Session\SaveHandler\SaveHandlerInterface as SaveHandler;
use Laminas\Session\Storage\StorageInterface as Storage;

/**
 * Base ManagerInterface implementation
 *
 * Defines common constructor logic and getters for Storage and Configuration
 */
abstract class AbstractManager implements Manager
{
    /**
     * @var Config
     */
    protected $config;

    /**
     * Default configuration class to use when no configuration provided
     * @var string
     */
    protected $defaultConfigClass = 'Laminas\Session\Config\SessionConfig';

    /**
     * @var Storage
     */
    protected $storage;

    /**
     * Default storage class to use when no storage provided
     * @var string
     */
    protected $defaultStorageClass = 'Laminas\Session\Storage\SessionArrayStorage';

    /**
     * @var SaveHandler
     */
    protected $saveHandler;

    /**
     * @var array
     */
    protected $validators;

    /**
     * Constructor
     *
     * @param  Config|null      $config
     * @param  Storage|null     $storage
     * @param  SaveHandler|null $saveHandler
     * @param  array            $validators
     * @throws Exception\RuntimeException
     */
    public function __construct(
        Config $config = null,
        Storage $storage = null,
        SaveHandler $saveHandler = null,
        array $validators = []
    ) {
        // init config
        if ($config === null) {
            if (! class_exists($this->defaultConfigClass)) {
                throw new Exception\RuntimeException(sprintf(
                    'Unable to locate config class "%s"; class does not exist',
                    $this->defaultConfigClass
                ));
            }

            $config = new $this->defaultConfigClass();

            if (! $config instanceof Config) {
                throw new Exception\RuntimeException(sprintf(
                    'Default config class %s is invalid; must implement %s\Config\ConfigInterface',
                    $this->defaultConfigClass,
                    __NAMESPACE__
                ));
            }
        }

        $this->config = $config;

        // init storage
        if ($storage === null) {
            if (! class_exists($this->defaultStorageClass)) {
                throw new Exception\RuntimeException(sprintf(
                    'Unable to locate storage class "%s"; class does not exist',
                    $this->defaultStorageClass
                ));
            }

            $storage = new $this->defaultStorageClass();

            if (! $storage instanceof Storage) {
                throw new Exception\RuntimeException(sprintf(
                    'Default storage class %s is invalid; must implement %s\Storage\StorageInterface',
                    $this->defaultConfigClass,
                    __NAMESPACE__
                ));
            }
        }

        $this->storage = $storage;

        // save handler
        if ($saveHandler !== null) {
            $this->saveHandler = $saveHandler;
        }

        $this->validators = $validators;
    }

    /**
     * Set configuration object
     *
     * @param  Config $config
     * @return AbstractManager
     */
    public function setConfig(Config $config)
    {
        $this->config = $config;
        return $this;
    }

    /**
     * Retrieve configuration object
     *
     * @return Config
     */
    public function getConfig()
    {
        return $this->config;
    }

    /**
     * Set session storage object
     *
     * @param  Storage $storage
     * @return AbstractManager
     */
    public function setStorage(Storage $storage)
    {
        $this->storage = $storage;
        return $this;
    }

    /**
     * Retrieve storage object
     *
     * @return Storage
     */
    public function getStorage()
    {
        return $this->storage;
    }

    /**
     * Set session save handler object
     *
     * @param  SaveHandler $saveHandler
     * @return AbstractManager
     */
    public function setSaveHandler(SaveHandler $saveHandler)
    {
        $this->saveHandler = $saveHandler;
        return $this;
    }

    /**
     * Get SaveHandler Object
     *
     * @return SaveHandler
     */
    public function getSaveHandler()
    {
        return $this->saveHandler;
    }
}
