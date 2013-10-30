<?php

namespace Log\Model;

class Log 
{

	public $logid;
	public $jobid;
	public $time;
	public $logtext;

	public function exchangeArray($data) 
	{
		$this->logid = (!empty($data['logid'])) ? $data['logid'] : null;
		$this->jobid = (!empty($data['jobid'])) ? $data['jobid'] : null;
		$this->time = (!empty($data['time'])) ? $data['time'] : null;
		$this->logtext = (!empty($data['logtext'])) ? $data['logtext'] : null;
	}

}

