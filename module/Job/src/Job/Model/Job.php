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

namespace Job\Model;

class Job 
{

	public $jobid;
	public $job;
	public $jobname;
	public $type;
	public $level;
	public $clientid;
	public $jobstatus;
	public $schedtime;
	public $starttime;
	public $endtime;
	public $realendtime;
	public $jobtdate;
	public $volsessionid;
	public $volsessiontime;
	public $jobfiles;
	public $jobbytes;
	public $readbytes;
	public $joberrors;
	public $jobmissingfiles; 	
	public $poolid;
	public $filesetid;
	public $priorjobid;
	public $purgedfiles;
	public $hasbase;
	public $hascache;
	public $reviewed;
	public $comment;
	public $clientname;
	public $duration;
	
	public function exchangeArray($data) 
	{
		$data = array_change_key_case($data, CASE_LOWER);

		$this->jobid = (!empty($data['jobid'])) ? $data['jobid'] : null;
		$this->job = (!empty($data['job'])) ? $data['job'] : null;
		$this->jobname = (!empty($data['name'])) ? $data['name'] : null;
		$this->type = (!empty($data['type'])) ? $data['type'] : null;
		$this->level = (!empty($data['level'])) ? $data['level'] : null;
		$this->clientid = (!empty($data['clientid'])) ? $data['clientid'] : null;
		$this->jobstatus = (!empty($data['jobstatus'])) ? $data['jobstatus'] : null;
		$this->schedtime = (!empty($data['schedtime'])) ? $data['schedtime'] : null;
		$this->starttime = (!empty($data['starttime'])) ? $data['starttime'] : null;
		$this->endtime = (!empty($data['endtime'])) ? $data['endtime'] : null;
		$this->realendtime = (!empty($data['realendtime'])) ? $data['realendtime'] : null;
		$this->jobdate = (!empty($data['jobdate'])) ? $data['jobdate'] : null;
		$this->volsessionid = (!empty($data['volsessionid'])) ? $data['volsessionid'] : null;
		$this->volsessiontime = (!empty($data['volsessiontime'])) ? $data['volsessiontime'] : null;
		$this->jobfiles = (!empty($data['jobfiles'])) ? $data['jobfiles'] : null;
		$this->jobbytes = (!empty($data['jobbytes'])) ? $data['jobbytes'] : null;
		$this->readbytes = (!empty($data['readbytes'])) ? $data['readbytes'] : null;
		$this->joberrors = (!empty($data['joberrors'])) ? $data['joberrors'] : null;
		$this->poolid = (!empty($data['poolid'])) ? $data['poolid'] : null;
		$this->filesetid = (!empty($data['filesetid'])) ? $data['filesetid'] : null;
		$this->clientname = (!empty($data['clientname'])) ? $data['clientname'] : null;
		$this->duration = (!empty($data['duration'])) ? $data['duration'] : null;
	}

}

