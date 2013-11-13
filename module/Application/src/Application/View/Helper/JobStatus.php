<?php

/**
 * namespace definition and usage
 */
namespace Application\View\Helper;

use Zend\View\Helper\AbstractHelper;

class JobStatus extends AbstractHelper
{
    public function __invoke($jobStatus)
    {
	switch($jobStatus)
	{
	    case 'e':
	      $output = '<span class="label label-danger">Non-fatal error</span>';
	      break;
	    case 'E':
	      $output = '<span class="label label-danger">Terminated with errors</span>';
	      break;
	    case 'f':
	      $output = '<span class="label label-danger">Fatal error</span>';
	      break;
	    case 'T':
	      $output = '<span class="label label-success">Success</span>';
	      break;
	    default:
	      $output = $jobStatus;
	      break;
	}
	
	return $output;
	
    }
}
