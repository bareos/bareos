<?php

/**
 * @see       https://github.com/laminas/laminas-console for the canonical source repository
 * @copyright https://github.com/laminas/laminas-console/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-console/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Console\Charset;

/**
 * Extended ASCII character set (positions 127+, MS DOS & Windows compatible)
 *
 * @link http://en.wikipedia.org/wiki/Box-drawing_characters
 */
class AsciiExtended implements CharsetInterface
{
    const ACTIVATE          = "";
    const DEACTIVATE        = "";

    const BLOCK             = "\xdb";
    const SHADE_LIGHT       = "\xb0";
    const SHADE_MEDIUM      = "\xb1";
    const SHADE_DARK        = "\xb2";

    const LINE_SINGLE_EW    = "\xc4";
    const LINE_SINGLE_NS    = "\xb3";
    const LINE_SINGLE_NW    = "\xda";
    const LINE_SINGLE_NE    = "\xbf";
    const LINE_SINGLE_SE    = "\xd9";
    const LINE_SINGLE_SW    = "\xc0";
    const LINE_SINGLE_CROSS = "\xc5";

    const LINE_DOUBLE_EW    = "\xcd";
    const LINE_DOUBLE_NS    = "\xba";
    const LINE_DOUBLE_NW    = "\xc9";
    const LINE_DOUBLE_NE    = "\xbb";
    const LINE_DOUBLE_SE    = "\xbc";
    const LINE_DOUBLE_SW    = "\xc8";
    const LINE_DOUBLE_CROSS = "\xce";

    const LINE_BLOCK_EW     = "\xdb";
    const LINE_BLOCK_NS     = "\xdb";
    const LINE_BLOCK_NW     = "\xdb";
    const LINE_BLOCK_NE     = "\xdb";
    const LINE_BLOCK_SE     = "\xdb";
    const LINE_BLOCK_SW     = "\xdb";
    const LINE_BLOCK_CROSS  = "\xdb";
}
