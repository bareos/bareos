<?php

namespace Admin\Model;

class Admin 
{

	public $userid;
	public $username;

	public function exchangeArray($data)
	{
		$this->userid = (!empty($data['userid'])) ? $data['userid'] : null;
		$this->username = (!empty($data['username'])) ? $data['username'] : null;
	}

}

