<?php

/**
 * @see       https://github.com/laminas/laminas-console for the canonical source repository
 * @copyright https://github.com/laminas/laminas-console/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-console/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Console\Color;

class Xterm256
{
    /**
     * Foreground constant
     */
    const FOREGROUND = 38;

    /**
     * Background constant
     */
    const BACKGROUND = 48;

    /**
     * @var string $color X11-formatted color value
     */
    public static $color;

    /**
     * Populate color property with X11-formatted equivalent
     *
     * @param mixed $color
     */
    protected function __construct($color = null)
    {
        static::$color = $color !== null ? sprintf('%%s;5;%s', $color) : null;
    }

    /**
     * Calcluate the X11 color value of a hexadecimal color
     *
     * @param  string $hexColor
     * @return self
     */
    public static function calculate($hexColor)
    {
        $hex = str_split($hexColor, 2);
        if (count($hex) !== 3 || !preg_match('#[0-9A-F]{6}#i', $hexColor)) {
            // Invalid/unknown color string
            return new static();
        }

        $ahex = array_map(function ($hex) {
            $val = round(((hexdec($hex) - 55)/40), 0);
            return $val > 0 ? (int) $val : 0;
        }, $hex);

        $dhex = array_map('hexdec', $hex);

        if (array_fill(0, 3, $dhex[0]) === $dhex && (int) substr($dhex[0], -1) === 8) {
            $x11 = 232 + (int) floor($dhex[0]/10);
            return new static($x11);
        }

        $x11 = $ahex[0] * 36 + $ahex[1] * 6 + $ahex[2] + 16;

        return new static($x11);
    }
}
