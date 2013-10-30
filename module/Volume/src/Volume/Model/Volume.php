<?php

namespace Volume\Model;

class Volume 
{

	public $mediaid;
	public $volumename;
	public $mediatype;
	public $lastwritten;
	public $volstatus;
	public $volretention;
	public $volbytes;
	public $maxvolbytes;
	public $storageid;

	public function exchangeArray($data)
	{
		$this->mediaid = (!empty($data['mediaid'])) ? $data['mediaid'] : null;
		$this->volumename = (!empty($data['volumename'])) ? $data['volumename'] : null;
		$this->mediatype = (!empty($data['mediatype'])) ? $data['mediatype'] : null;
		$this->lastwritten = (!empty($data['lastwritten'])) ? $data['lastwritten'] : null;
		$this->volstatus = (!empty($data['volstatus'])) ? $data['volstatus'] : null;
		$this->volretention = (!empty($data['volretention'])) ? $data['volretention'] : null;
		$this->volbytes = (!empty($data['volbytes'])) ? $data['volbytes'] : null;
		$this->maxvolbytes = (!empty($data['maxvolbytes'])) ? $data['maxvolbytes'] : null;
		$this->storageid = (!empty($data['storageid'])) ? $data['storageid'] : null;
	}

}

