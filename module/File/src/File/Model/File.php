<?php

namespace File\Model;

class File 
{

	public $fileid;
	public $jobid;
	public $pathid;
	public $filenameid;
	public $filename;

	public function exchangeArray($data)
	{
		$this->fileid = (!empty($data['fileid'])) ? $data['fileid'] : null;
		$this->jobid = (!empty($data['jobid'])) ? $data['jobid'] : null;
		$this->pathid = (!empty($data['pathid'])) ? $data['pathid'] : null;
		$this->filenameid = (!empty($data['filenameid'])) ? $data['filenameid'] : null;
		$this->name = (!empty($data['name'])) ? $data['name'] : null;
	}

}

