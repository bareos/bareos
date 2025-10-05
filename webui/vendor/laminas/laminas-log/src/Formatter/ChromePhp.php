<?php

/**
 * @see       https://github.com/laminas/laminas-log for the canonical source repository
 * @copyright https://github.com/laminas/laminas-log/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-log/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Log\Formatter;

class ChromePhp implements FormatterInterface
{
    /**
     * Formats the given event data into a single line to be written by the writer.
     *
     * @param array $event The event data which should be formatted.
     * @return string
     */
    public function format($event)
    {
        return $event['message'];
    }

    /**
     * This method is implemented for FormatterInterface but not used.
     *
     * @return string
     */
    public function getDateTimeFormat()
    {
        return '';
    }

    /**
     * This method is implemented for FormatterInterface but not used.
     *
     * @param string $dateTimeFormat
     * @return FormatterInterface
     */
    public function setDateTimeFormat($dateTimeFormat)
    {
        return $this;
    }
}
