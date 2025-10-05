<?php

/**
 * @see       https://github.com/laminas/laminas-cache for the canonical source repository
 * @copyright https://github.com/laminas/laminas-cache/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-cache/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Cache\Storage;

interface TaggableInterface
{
    /**
     * Set tags to an item by given key.
     * An empty array will remove all tags.
     *
     * @param string   $key
     * @param string[] $tags
     * @return bool
     */
    public function setTags($key, array $tags);

    /**
     * Get tags of an item by given key
     *
     * @param string $key
     * @return string[]|FALSE
     */
    public function getTags($key);

    /**
     * Remove items matching given tags.
     *
     * If $disjunction only one of the given tags must match
     * else all given tags must match.
     *
     * @param string[] $tags
     * @param  bool  $disjunction
     * @return bool
     */
    public function clearByTags(array $tags, $disjunction = false);
}
