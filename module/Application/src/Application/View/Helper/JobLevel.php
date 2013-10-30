<?php

/**
 * namespace definition and usage
 */
namespace Application\View\Helper;

use Zend\View\Helper\AbstractHelper;

class JobLevel extends AbstractHelper
{
    public function __invoke($jobLevel)
    {
	switch($jobLevel)
	{
	    case 'I':
	      $output = "Incremental";
	      break;
	    case 'D':
	      $output = "Differential";
	      break;
	    case 'F':
	      $output = "Full";
	      break;
	    default:
	      $output = $jobLevel;
	      break;
	}
	
	return $output;
	
    }
}