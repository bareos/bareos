<?php

/**
 * namespace definition and usage
 */
namespace Application\View\Helper;

use Zend\View\Helper\AbstractHelper;

class JobDuration extends AbstractHelper
{
    public function __invoke($date1, $date2)
    {
	$datetime1 = strtotime($date1);
	$datetime2 = strtotime($date2);
	$diff = $datetime2 - $datetime1;
	$duration = date('H:i:s',$diff);
	return $duration;
    }
}
