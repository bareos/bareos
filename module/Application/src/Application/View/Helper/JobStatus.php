<?php

/**
 *
 * bareos-webui - Bareos Web-Frontend
 * 
 * @link      https://github.com/bareos/bareos-webui for the canonical source repository
 * @copyright Copyright (c) 2013-2014 dass-IT GmbH (http://www.dass-it.de/)
 * @license   GNU Affero General Public License (http://www.gnu.org/licenses/)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
 
namespace Application\View\Helper;

use Zend\View\Helper\AbstractHelper;

class JobStatus extends AbstractHelper
{

    public function __invoke($jobStatus)
    {

		switch($jobStatus)
		{
			// Non-fatal error
	    	case 'e':
	      		$output = '<span class="label label-danger">Failure</span>';
	      		break;
			// Terminated with errors
	    	case 'E':
	      		$output = '<span class="label label-danger">Failure</span>';
	      		break;
			// Fatal error
	    	case 'f':
	      		$output = '<span class="label label-danger">Failure</span>';
	      		break;
			// Terminated successful
	    	case 'T':
	      		$output = '<span class="label label-success">Success</span>';
	      		break;
			// Running
	    	case 'R':
	      		$output = '<span class="label label-info">Running</span>';
	      		break;
			// Created no yet running
	    	case 'C':
	      		$output = '<span class="label label-default">Queued</span>';
	      		break;
	    	// Blocked
			case 'B':
	      		$output = '<span class="label label-warning">Blocked</span>';
	      		break;
			// Verify found differences
	    	case 'D':
	      		$output = '<span class="label label-warning">Verify found differences</span>';
	      		break;
			// Canceled by user
	    	case 'A':
	      		$output = '<span class="label label-warning">Canceled</span>';
	      		break;
			// Waiting for client
	    	case 'F':
	      		$output = '<span class="label label-default">Waiting</span>';
	      		break;
			// Waiting for storage daemon
	    	case 'S':
	      		$output = '<span class="label label-default">Waiting</span>';
	      		break;
			// Waiting for new media
	    	case 'm':
	      		$output = '<span class="label label-default">Waiting</span>';
	      		break;
			// Waiting for media mount
	    	case 'M':
	      		$output = '<span class="label label-default">Waiting</span>';
	      		break;
			// Waiting for storage resource
	    	case 's':
	      		$output = '<span class="label label-default">Waiting</span>';
	      		break;
			// Waiting for job resource
	    	case 'j':
	      		$output = '<span class="label label-default">Waiting</span>';
	      		break;
			// Waiting for client resource
	    	case 'c':
	      		$output = '<span class="label label-default">Waiting</span>';
	      		break;
			// Waiting on maximum jobs
	    	case 'd':
	      		$output = '<span class="label label-default">Waiting</span>';
	      		break;
			// Waiting on starttime
	    	case 't':
	      		$output = '<span class="label label-default">Waiting</span>';
	      		break;
			// Waiting on higher priority jobs
	    	case 'p':
	      		$output = '<span class="label label-default">Waiting</span>';
	      		break;
			// SD despooling attributes
	    	case 'a':
	      		$output = '<span class="label label-info">SD despooling attributes</span>';
	      		break;
			// Doing batch insert file records
	    	case 'i':
	      		$output = '<span class="label label-info">Doing batch insert file records</span>';
	      		break;
			// Incomplete
	    	case 'I':
              	$output = '<span class="label label-primary">Incomplete</span>';
				break;
			// Committing data
	    	case 'L':
              	$output = '<span class="label label-info">Committing data</span>';
            	break;
			// Terminated with warnings
	    	case 'W':
              	$output = '<span class="label label-warning">Warning</span>';
              	break;
			// Doing data despooling 
	    	case 'l':
              	$output = '<span class="label label-info">Doing data despooling</span>';
              	break;
			// Queued waiting for device
	    	case 'q':
              	$output = '<span class="label label-default">Queued waiting for device</span>';
              	break;
			// Default
	    	default:
				$output = '<span class="label label-primary">' . $jobStatus . '</span>';
	      		break;
		}
	
		return $output;
	
    }

}
