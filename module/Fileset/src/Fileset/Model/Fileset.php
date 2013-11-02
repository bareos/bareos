<?php

namespace Fileset\Model;

class Fileset 
{

	public $filesetid;
	public $fileset;
	public $md5;
	public $createtime;

	public function exchangeArray($data)
	{
		$this->filesetid = (!empty($data['filesetid'])) ? $data['filesetid'] : null;
		$this->fileset = (!empty($data['fileset'])) ? $data['fileset'] : null;
		$this->md5 = (!empty($data['md5'])) ? $data['md5'] : null;
		$this->createtime = (!empty($data['createtime'])) ? $data['createtime'] : null;
	}

}

