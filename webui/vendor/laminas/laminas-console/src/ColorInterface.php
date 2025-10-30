<?php

/**
 * @see       https://github.com/laminas/laminas-console for the canonical source repository
 * @copyright https://github.com/laminas/laminas-console/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-console/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Console;

interface ColorInterface
{
    const NORMAL = 0;
    const RESET = 0;

    const BLACK = 1;
    const RED = 2;
    const GREEN = 3;
    const YELLOW = 4;
    const BLUE = 5;
    const MAGENTA = 6;
    const CYAN = 7;
    const WHITE = 8;

    const GRAY = 9;
    const LIGHT_RED = 10;
    const LIGHT_GREEN = 11;
    const LIGHT_YELLOW = 12;
    const LIGHT_BLUE = 13;
    const LIGHT_MAGENTA = 14;
    const LIGHT_CYAN = 15;
    const LIGHT_WHITE = 16;
}
