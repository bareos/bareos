<?php

namespace Pool\Model;

class Pool 
{

	public $poolid;
	public $name;
	public $pooltype;

	public function exchangeArray($data)
	{
		$this->poolid = (!empty($data['poolid'])) ? $data['poolid'] : null;
		$this->name = (!empty($data['name'])) ? $data['name'] : null;
		$this->pooltype = (!empty($data['pooltype'])) ? $data['pooltype'] : null;
	}

}

