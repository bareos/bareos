<?php

/**
 * namespace definition and usage
 */
namespace Application\View\Helper;

use Zend\View\Helper\AbstractHelper;

class JobType extends AbstractHelper
{
    public function __invoke($jobType)
    {
	switch($jobType)
	{
	    case 'B':
	      $output = "Backup";
	      break;
	    case 'R':
	      $output = "Restore";
	      break;
	    default:
	      $output = $jobType;
	      break;
	}
	
	return $output;
	
    }
}
