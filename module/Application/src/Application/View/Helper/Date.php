<?php
/**
 * ZF2 Buch Kapitel 15
 * 
 * Das Buch "Zend Framework 2 - Das Praxisbuch"
 * von Ralf Eggert ist im Galileo-Computing Verlag erschienen. 
 * ISBN 978-3-8362-2610-3
 * 
 * @package    Application
 * @author     Ralf Eggert <r.eggert@travello.de>
 * @copyright  Alle Listings sind urheberrechtlich geschützt!
 * @link       http://www.zendframeworkbuch.de/ und http://www.galileocomputing.de/3460
 */

/**
 * namespace definition and usage
 */
namespace Application\View\Helper;

use DateTime;
use IntlDateFormatter;
use Zend\View\Helper\AbstractHelper;

/**
 * Date output
 * 
 * Simplifies the date output for the dateFormat view helper
 * 
 * @package    Application
 */
class Date extends AbstractHelper
{
    /**
     * get string date and output it
     * 
     * @param string $dateString
     * @param string $mode
     * @return boolean
     */
    public function __invoke($dateString, $mode = 'medium')
    {
        if ($dateString == '0000-00-00 00:00:00' || $dateString == '') {
            return '-';
        }
        
        switch ($mode) {
            case 'long':
                $dateType = IntlDateFormatter::LONG;
                $timeType = IntlDateFormatter::LONG;
                break;

            case 'short':
                $dateType = IntlDateFormatter::SHORT;
                $timeType = IntlDateFormatter::SHORT;
                break;

            case 'dateonly':
                $dateType = IntlDateFormatter::MEDIUM;
                $timeType = IntlDateFormatter::NONE;
                break;

            default:
            case 'medium':
                $dateType = IntlDateFormatter::MEDIUM;
                $timeType = IntlDateFormatter::MEDIUM;
                break;
        }
        
        $dateTime = new DateTime($dateString);
        
        return $this->getView()->dateFormat($dateTime, $dateType, $timeType);
    }
}
