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
	    case 'R':
	      $output = '<span class="label label-info">Running</span>';
	      break;
	    case 'C':
	      $output = '<span class="label label-default">Created no yet running</span>';
	      break;
	    case 'B':
	      $output = '<span class="label label-warning">Blocked</span>';
	      break;
	    case 'D':
	      $output = '<span class="label label-warning">Verify found differences</span>';
	      break;
	    case 'A':
	      $output = '<span class="label label-warning">Canceled by user</span>';
	      break;
	    case 'F':
	      $output = '<span class="label label-default">Waiting for client</span>';
	      break;
	    case 'S':
	      $output = '<span class="label label-default">Waiting for storage daemon</span>';
	      break;
	    case 'm':
	      $output = '<span class="label label-default">Waiting for new media</span>';
	      break;
	    case 'M':
	      $output = '<span class="label label-default">Waiting for media mount</span>';
	      break;
	    case 's':
	      $output = '<span class="label label-default">Waiting for storage resource</span>';
	      break;
	    case 'j':
	      $output = '<span class="label label-default">Waiting for job resource</span>';
	      break;
	    case 'c':
	      $output = '<span class="label label-default">Waiting for client resource</span>';
	      break;
	    case 'd':
	      $output = '<span class="label label-default">Waiting on maximum jobs</span>';
	      break;
	    case 't':
	      $output = '<span class="label label-default">Waiting on starttime</span>';
	      break;
	    case 'p':
	      $output = '<span class="label label-default">Waiting on higher priority jobs</span>';
	      break;
	    case 'a':
	      $output = '<span class="label label-info">SD despooling attributes</span>';
	      break;
	    case 'i':
	      $output = '<span class="label label-info">Doing batch insert file records</span>';
	      break;
	    default:
	      $output = '<span class="label label-primary">' . $jobStatus . '</span>';
	      break;
	}
	
	return $output;
	
    }
}
