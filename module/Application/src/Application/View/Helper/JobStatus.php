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
	      $output = "<div class='text-danger'>Non-fatal error</div>";
	      break;
	    case 'E':
	      $output = "<div class='text-danger'>Terminated with errors</div>";
	      break;
	    case 'f':
	      $output = "<div class='text-danger'>Fatal error</div>";
	      break;
	    case 'T':
	      $output = "<div class='text-success'>Completed successfully</div>";
	      break;
	    default:
	      $output = $jobStatus;
	      break;
	}
	
	return $output;
	
    }
}
