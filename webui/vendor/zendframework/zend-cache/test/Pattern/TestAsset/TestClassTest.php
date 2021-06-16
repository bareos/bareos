<?php

namespace ZendTest\Cache\Pattern\TestAsset;

/**
 * Test class
 */
class TestClassCache
{
    /**
     * A counter how oftern the method "bar" was called
     */
    public static $fooCounter = 0;

    public static function bar()
    {
        ++static::$fooCounter;
        $args = func_get_args();

        echo 'foobar_output('.implode(', ', $args) . ') : ' . static::$fooCounter;
        return 'foobar_return('.implode(', ', $args) . ') : ' . static::$fooCounter;
    }

    public static function emptyMethod()
    {
    }
}
