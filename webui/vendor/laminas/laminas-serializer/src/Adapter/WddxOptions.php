<?php

/**
 * @see       https://github.com/laminas/laminas-serializer for the canonical source repository
 * @copyright https://github.com/laminas/laminas-serializer/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-serializer/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Serializer\Adapter;

class WddxOptions extends AdapterOptions
{
    /**
     * Wddx packet header comment
     *
     * @var string
     */
    protected $comment = '';

    /**
     * Set WDDX header comment
     *
     * @param  string $comment
     * @return WddxOptions
     */
    public function setComment($comment)
    {
        $this->comment = (string) $comment;
        return $this;
    }

    /**
     * Get WDDX header comment
     *
     * @return string
     */
    public function getComment()
    {
        return $this->comment;
    }
}
