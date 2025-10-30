<?php

/**
 * @see       https://github.com/laminas/laminas-log for the canonical source repository
 * @copyright https://github.com/laminas/laminas-log/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-log/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Log\Formatter;

use DateTime;
use Traversable;

class Db implements FormatterInterface
{
    /**
     * Format specifier for DateTime objects in event data (default: ISO 8601)
     *
     * @see http://php.net/manual/en/function.date.php
     * @var string
     */
    protected $dateTimeFormat = self::DEFAULT_DATETIME_FORMAT;

    /**
     * Class constructor
     *
     * @see http://php.net/manual/en/function.date.php
     * @param null|string $dateTimeFormat Format specifier for DateTime objects in event data
     */
    public function __construct($dateTimeFormat = null)
    {
        if ($dateTimeFormat instanceof Traversable) {
            $dateTimeFormat = iterator_to_array($dateTimeFormat);
        }

        if (is_array($dateTimeFormat)) {
            $dateTimeFormat = isset($dateTimeFormat['dateTimeFormat'])? $dateTimeFormat['dateTimeFormat'] : null;
        }

        if (null !== $dateTimeFormat) {
            $this->setDateTimeFormat($dateTimeFormat);
        }
    }

    /**
     * Formats data to be written by the writer.
     *
     * @param array $event event data
     * @return array
     */
    public function format($event)
    {
        $format = $this->getDateTimeFormat();
        array_walk_recursive($event, function (&$value) use ($format) {
            if ($value instanceof DateTime) {
                $value = $value->format($format);
            }
        });

        return $event;
    }

    /**
     * {@inheritDoc}
     */
    public function getDateTimeFormat()
    {
        return $this->dateTimeFormat;
    }

    /**
     * {@inheritDoc}
     */
    public function setDateTimeFormat($dateTimeFormat)
    {
        $this->dateTimeFormat = (string) $dateTimeFormat;
        return $this;
    }
}
