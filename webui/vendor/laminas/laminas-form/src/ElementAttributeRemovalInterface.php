<?php

/**
 * @see       https://github.com/laminas/laminas-form for the canonical source repository
 * @copyright https://github.com/laminas/laminas-form/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-form/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Form;

interface ElementAttributeRemovalInterface
{
    /**
     * Remove a single element attribute
     *
     * @param  string $key
     * @return ElementAttributeRemovalInterface
     */
    public function removeAttribute($key);

    /**
     * Remove many attributes at once
     *
     * @param array $keys
     * @return ElementAttributeRemovalInterface
     */
    public function removeAttributes(array $keys);

    /**
     * Remove all attributes at once
     *
     * @return ElementAttributeRemovalInterface
     */
    public function clearAttributes();
}
