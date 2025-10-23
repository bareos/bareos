<?php

/**
 * @see       https://github.com/laminas/laminas-hydrator for the canonical source repository
 * @copyright https://github.com/laminas/laminas-hydrator/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-hydrator/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Hydrator\Strategy;

use DateTime;
use DateTimeZone;

class DateTimeFormatterStrategy implements StrategyInterface
{
    /**
     * @var string
     */
    private $format;

    /**
     * @var DateTimeZone|null
     */
    private $timezone;

    /**
     * Constructor
     *
     * @param string            $format
     * @param DateTimeZone|null $timezone
     */
    public function __construct($format = DateTime::RFC3339, DateTimeZone $timezone = null)
    {
        $this->format   = (string) $format;
        $this->timezone = $timezone;
    }

    /**
     * {@inheritDoc}
     *
     * Converts to date time string
     *
     * @param mixed|DateTime $value
     *
     * @return mixed|string
     */
    public function extract($value)
    {
        if ($value instanceof DateTime) {
            return $value->format($this->format);
        }

        return $value;
    }

    /**
     * Converts date time string to DateTime instance for injecting to object
     *
     * {@inheritDoc}
     *
     * @param mixed|string $value
     *
     * @return mixed|DateTime
     */
    public function hydrate($value)
    {
        if ($value === '' || $value === null) {
            return;
        }

        if ($this->timezone) {
            $hydrated = DateTime::createFromFormat($this->format, $value, $this->timezone);
        } else {
            $hydrated = DateTime::createFromFormat($this->format, $value);
        }

        return $hydrated ?: $value;
    }
}
