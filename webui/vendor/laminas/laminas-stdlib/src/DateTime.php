<?php

/**
 * @see       https://github.com/laminas/laminas-stdlib for the canonical source repository
 * @copyright https://github.com/laminas/laminas-stdlib/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-stdlib/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Stdlib;

use DateTimeZone;

trigger_error('DateTime extension deprecated as of Laminas 2.1.4; use the \DateTime constructor to parse extended ISO8601 dates instead', E_USER_DEPRECATED);

/**
 * DateTime
 *
 * An extension of the \DateTime object.
 *
 * @deprecated
 */
class DateTime extends \DateTime
{
    /**
     * The DateTime::ISO8601 constant used by php's native DateTime object does
     * not allow for fractions of a second. This function better handles ISO8601
     * formatted date strings.
     *
     * @param  string       $time
     * @param  DateTimeZone $timezone
     * @return mixed
     */
    public static function createFromISO8601($time, DateTimeZone $timezone = null)
    {
        $format = self::ISO8601;
        if (isset($time[19]) && $time[19] === '.') {
            $format = 'Y-m-d\TH:i:s.uO';
        }

        if ($timezone !== null) {
            return self::createFromFormat($format, $time, $timezone);
        }

        return self::createFromFormat($format, $time);
    }
}
