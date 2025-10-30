<?php

/**
 * @see       https://github.com/laminas/laminas-log for the canonical source repository
 * @copyright https://github.com/laminas/laminas-log/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-log/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Log\Writer;

use Laminas\Log\Filter\FilterInterface as Filter;
use Laminas\Log\Formatter\FormatterInterface as Formatter;

interface WriterInterface
{
    /**
     * Add a log filter to the writer
     *
     * @param  int|string|Filter $filter
     * @return WriterInterface
     */
    public function addFilter($filter);

    /**
     * Set a message formatter for the writer
     *
     * @param string|Formatter $formatter
     * @return WriterInterface
     */
    public function setFormatter($formatter);

    /**
     * Write a log message
     *
     * @param  array $event
     * @return WriterInterface
     */
    public function write(array $event);

    /**
     * Perform shutdown activities
     *
     * @return void
     */
    public function shutdown();
}
