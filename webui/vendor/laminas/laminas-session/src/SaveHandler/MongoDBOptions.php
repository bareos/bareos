<?php

/**
 * @see       https://github.com/laminas/laminas-session for the canonical source repository
 * @copyright https://github.com/laminas/laminas-session/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-session/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Session\SaveHandler;

use Laminas\Session\Exception\InvalidArgumentException;
use Laminas\Stdlib\AbstractOptions;

/**
 * MongoDB session save handler Options
 */
class MongoDBOptions extends AbstractOptions
{
    /**
     * Database name
     *
     * @var string
     */
    protected $database;

    /**
     * Collection name
     *
     * @var string
     */
    protected $collection;

    /**
     * Save options
     *
     * @see http://php.net/manual/en/mongocollection.save.php
     * @var string
     */
    protected $saveOptions = ['w' => 1];

    /**
     * Name field
     *
     * @var string
     */
    protected $nameField = 'name';

    /**
     * Data field
     *
     * @var string
     */
    protected $dataField = 'data';

    /**
     * Lifetime field
     *
     * @var string
     */
    protected $lifetimeField = 'lifetime';

    /**
     * Modified field
     *
     * @var string
     */
    protected $modifiedField = 'modified';

    /**
     * Use expireAfterSeconds index
     *
     * @var bool
     */
    protected $useExpireAfterSecondsIndex = false;

    /**
     * {@inheritdoc}
     */
    public function __construct($options = null)
    {
        parent::__construct($options);

        $mongoVersion = phpversion('mongo') ?: '0.0.0';
        if ($this->saveOptions === ['w' => 1] && version_compare($mongoVersion, '1.3.0', '<')) {
            $this->saveOptions = ['safe' => true];
        }
    }

    /**
     * Override AbstractOptions::__set
     *
     * Validates value if save options are being set.
     *
     * @param string $key
     * @param mixed $value
     */
    public function __set($key, $value)
    {
        if (strtolower($key) !== 'saveoptions') {
            return parent::__set($key, $value);
        }

        if (! is_array($value)) {
            throw new InvalidArgumentException('Expected array for save options');
        }
        $this->setSaveOptions($value);
    }

    /**
     * Set database name
     *
     * @param string $database
     * @return MongoDBOptions
     * @throws InvalidArgumentException
     */
    public function setDatabase($database)
    {
        $database = (string) $database;
        if (strlen($database) === 0) {
            throw new InvalidArgumentException('$database must be a non-empty string');
        }
        $this->database = $database;
        return $this;
    }

    /**
     * Get database name
     *
     * @return string
     */
    public function getDatabase()
    {
        return $this->database;
    }

    /**
     * Set collection name
     *
     * @param string $collection
     * @return MongoDBOptions
     * @throws InvalidArgumentException
     */
    public function setCollection($collection)
    {
        $collection = (string) $collection;
        if (strlen($collection) === 0) {
            throw new InvalidArgumentException('$collection must be a non-empty string');
        }
        $this->collection = $collection;
        return $this;
    }

    /**
     * Get collection name
     *
     * @return string
     */
    public function getCollection()
    {
        return $this->collection;
    }

    /**
     * Set save options
     *
     * @see http://php.net/manual/en/mongocollection.save.php
     * @param array $saveOptions
     * @return MongoDBOptions
     */
    public function setSaveOptions(array $saveOptions)
    {
        $this->saveOptions = $saveOptions;
        return $this;
    }

    /**
     * Get save options
     *
     * @return string
     */
    public function getSaveOptions()
    {
        return $this->saveOptions;
    }

    /**
     * Set name field
     *
     * @param string $nameField
     * @return MongoDBOptions
     * @throws InvalidArgumentException
     */
    public function setNameField($nameField)
    {
        $nameField = (string) $nameField;
        if (strlen($nameField) === 0) {
            throw new InvalidArgumentException('$nameField must be a non-empty string');
        }
        $this->nameField = $nameField;
        return $this;
    }

    /**
     * Get name field
     *
     * @return string
     */
    public function getNameField()
    {
        return $this->nameField;
    }

    /**
     * Set data field
     *
     * @param string $dataField
     * @return MongoDBOptions
     * @throws InvalidArgumentException
     */
    public function setDataField($dataField)
    {
        $dataField = (string) $dataField;
        if (strlen($dataField) === 0) {
            throw new InvalidArgumentException('$dataField must be a non-empty string');
        }
        $this->dataField = $dataField;
        return $this;
    }

    /**
     * Get data field
     *
     * @return string
     */
    public function getDataField()
    {
        return $this->dataField;
    }

    /**
     * Set lifetime field
     *
     * @param string $lifetimeField
     * @return MongoDBOptions
     * @throws InvalidArgumentException
     */
    public function setLifetimeField($lifetimeField)
    {
        $lifetimeField = (string) $lifetimeField;
        if (strlen($lifetimeField) === 0) {
            throw new InvalidArgumentException('$lifetimeField must be a non-empty string');
        }
        $this->lifetimeField = $lifetimeField;
        return $this;
    }

    /**
     * Get lifetime Field
     *
     * @return string
     */
    public function getLifetimeField()
    {
        return $this->lifetimeField;
    }

    /**
     * Set Modified Field
     *
     * @param string $modifiedField
     * @return MongoDBOptions
     * @throws InvalidArgumentException
     */
    public function setModifiedField($modifiedField)
    {
        $modifiedField = (string) $modifiedField;
        if (strlen($modifiedField) === 0) {
            throw new InvalidArgumentException('$modifiedField must be a non-empty string');
        }
        $this->modifiedField = $modifiedField;
        return $this;
    }

    /**
     * Get modified Field
     *
     * @return string
     */
    public function getModifiedField()
    {
        return $this->modifiedField;
    }

    /**
     * @return boolean
     */
    public function useExpireAfterSecondsIndex()
    {
        return $this->useExpireAfterSecondsIndex;
    }

    /**
     * Enable expireAfterSeconds index.
     *
     * @see http://docs.mongodb.org/manual/tutorial/expire-data/
     * @param boolean $useExpireAfterSecondsIndex
     */
    public function setUseExpireAfterSecondsIndex($useExpireAfterSecondsIndex)
    {
        $this->useExpireAfterSecondsIndex = (bool) $useExpireAfterSecondsIndex;
    }
}
