<?php

/**
 * @see       https://github.com/laminas/laminas-eventmanager for the canonical source repository
 * @copyright https://github.com/laminas/laminas-eventmanager/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-eventmanager/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\EventManager\Filter;

use Laminas\EventManager\ResponseCollection;
use Laminas\Stdlib\CallbackHandler;

/**
 * Interface for intercepting filter chains
 */
interface FilterInterface
{
    /**
     * Execute the filter chain
     *
     * @param  string|object $context
     * @param  array $params
     * @return mixed
     */
    public function run($context, array $params = []);

    /**
     * Attach an intercepting filter
     *
     * @param  callable $callback
     * @return CallbackHandler
     */
    public function attach($callback);

    /**
     * Detach an intercepting filter
     *
     * @param  CallbackHandler $filter
     * @return bool
     */
    public function detach(CallbackHandler $filter);

    /**
     * Get all intercepting filters
     *
     * @return array
     */
    public function getFilters();

    /**
     * Clear all filters
     *
     * @return void
     */
    public function clearFilters();

    /**
     * Get all filter responses
     *
     * @return ResponseCollection
     */
    public function getResponses();
}
