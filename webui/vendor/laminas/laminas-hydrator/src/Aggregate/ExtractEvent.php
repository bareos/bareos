<?php

/**
 * @see       https://github.com/laminas/laminas-hydrator for the canonical source repository
 * @copyright https://github.com/laminas/laminas-hydrator/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-hydrator/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Hydrator\Aggregate;

use Laminas\EventManager\Event;

/**
 * Event triggered when the {@see AggregateHydrator} extracts
 * data from an object
 */
class ExtractEvent extends Event
{
    const EVENT_EXTRACT = 'extract';

    /**
     * {@inheritDoc}
     */
    protected $name = self::EVENT_EXTRACT;

    /**
     * @var object
     */
    protected $extractionObject;

    /**
     * @var array
     */
    protected $extractedData = [];

    /**
     * @param object $target
     * @param object $extractionObject
     */
    public function __construct($target, $extractionObject)
    {
        $this->target           = $target;
        $this->extractionObject = $extractionObject;
    }

    /**
     * Retrieves the object from which data is extracted
     *
     * @return object
     */
    public function getExtractionObject()
    {
        return $this->extractionObject;
    }

    /**
     * @param object $extractionObject
     *
     * @return void
     */
    public function setExtractionObject($extractionObject)
    {
        $this->extractionObject = $extractionObject;
    }

    /**
     * Retrieves the data that has been extracted
     *
     * @return array
     */
    public function getExtractedData()
    {
        return $this->extractedData;
    }

    /**
     * @param array $extractedData
     *
     * @return void
     */
    public function setExtractedData(array $extractedData)
    {
        $this->extractedData = $extractedData;
    }

    /**
     * Merge provided data with the extracted data
     *
     * @param array $additionalData
     *
     * @return void
     */
    public function mergeExtractedData(array $additionalData)
    {
        $this->extractedData = array_merge($this->extractedData, $additionalData);
    }
}
