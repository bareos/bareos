<?php

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
	
	public function exchangeArray($data) 
	{
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
	}

}

