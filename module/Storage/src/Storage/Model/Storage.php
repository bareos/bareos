<?php

namespace Storage\Model;

class Storage 
{

	public $storageid;
	public $name;
	public $autochanger;

	public function exchangeArray($data)
	{
		$this->storageid = (!empty($data['storageid'])) ? $data['storageid'] : null;
		$this->name = (!empty($data['name'])) ? $data['name'] : null;
		$this->autochanger = (!empty($data['autochanger'])) ? $data['autochanger'] : null;
	}

}

