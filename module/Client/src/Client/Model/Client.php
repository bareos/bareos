<?php

namespace Client\Model;

class Client 
{

	public $clientid;
	public $name;
	public $uname;
	public $autoprune;
	public $fileretention;
	public $jobretention;

	public function exchangeArray($data)
	{
		$data = array_change_key_case($data, CASE_LOWER);

		$this->clientid = (!empty($data['clientid'])) ? $data['clientid'] : null;
		$this->name = (!empty($data['name'])) ? $data['name'] : null;
		$this->uname = (!empty($data['uname'])) ? $data['uname'] : null;
		$this->autoprune = (!empty($data['autoprune'])) ? $data['autoprune'] : null;
		$this->fileretention = (!empty($data['fileretention'])) ? $data['fileretention'] : null;
		$this->jobretention = (!empty($data['jobretention'])) ? $data['jobretention'] : null;
	}

}

