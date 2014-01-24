<?php

/**
 *
 * Barbossa - A Web-Frontend to manage Bareos
 * 
 * @link      http://github.com/fbergkemper/barbossa for the canonical source repository
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

namespace Media\Model;

class Media 
{

	public $mediaid;
	public $volumename;
	public $slot;
	public $poolid;
	public $mediatype;
	public $mediatypeid;
	public $labeltype;
	public $firstwritten;
	public $lastwritten;
	public $labeldate;
	public $voljobs;
	public $volfiles;
	public $volblocks;
	public $volmounts;
	public $volbytes;
	public $volerrors;
	public $volwrites;
	public $volcapacitybytes;
	public $volstatus;
	public $enabled;
	public $recycle;
	public $actiononpurge;
	public $volretention;
	public $voluseduration;
	public $maxvoljobs;
	public $maxvolfiles;
	public $maxvolbytes;
	public $inchanger;
	public $storageid;
	public $deviceid;
	public $mediaaddressing;
	public $volreadtime;
	public $volwritetime;
	public $endfile;
	public $endblock;
	public $locationid;
	public $recyclecount;
	public $initialwrite;
	public $scratchpoolid;
	public $recyclepoolid;
	public $encryptionkey;
	public $comment;

	public function exchangeArray($data)
	{
		$this->mediaid = (!empty($data['mediaid'])) ? $data['mediaid'] : null;
		$this->volumename = (!empty($data['volumename'])) ? $data['volumename'] : null;
		$this->slot = (!empty($data['slot'])) ? $data['slot'] : null;
		$this->poolid = (!empty($data['poolid'])) ? $data['poolid'] : null;
		$this->mediatype = (!empty($data['mediatype'])) ? $data['mediatype'] : null;
		$this->mediatypeid = (!empty($data['mediatypeid'])) ? $data['mediatypeid'] : null;
		$this->labeltype = (!empty($data['labeltype'])) ? $data['labeltype'] : null;
		$this->firstwritten = (!empty($data['firstwritten'])) ? $data['firstwritten'] : null;
		$this->lastwritten = (!empty($data['lastwritten'])) ? $data['lastwritten'] : null;
		$this->labeldate = (!empty($data['labeldate'])) ? $data['labeldate'] : null;
		$this->voljobs = (!empty($data['voljobs'])) ? $data['voljobs'] : null;
		$this->volfiles = (!empty($data['volfiles'])) ? $data['volfiles'] : null;
		$this->volblocks = (!empty($data['volblocks'])) ? $data['volblocks'] : null;
		$this->volmounts = (!empty($data['volmounts'])) ? $data['volmounts'] : null;
		$this->volbytes = (!empty($data['volbytes'])) ? $data['volbytes'] : null;
		$this->volerrors = (!empty($data['volerrors'])) ? $data['volerrors'] : null;
		$this->volwrites = (!empty($data['volwrites'])) ? $data['volwrites'] : null;
		$this->volcapacitybytes = (!empty($data['volcapacitybytes'])) ? $data['volcapacitybytes'] : null;
		$this->volstatus = (!empty($data['volstatus'])) ? $data['volstatus'] : null;
		$this->enabled = (!empty($data['enabled'])) ? $data['enabled'] : null;
		$this->recycle = (!empty($data['recycle'])) ? $data['recycle'] : null;
		$this->actiononpurge = (!empty($data['actiononpurge'])) ? $data['actiononpurge'] : null;
		$this->volretention = (!empty($data['volretention'])) ? $data['volretention'] : null;
		$this->voluseduration = (!empty($data['voluseduration'])) ? $data['voluseduration'] : null;
		$this->maxvoljobs = (!empty($data['maxvoljobs'])) ? $data['maxvoljobs'] : null;
		$this->maxvolfiles = (!empty($data['maxvolfiles'])) ? $data['maxvolfiles'] : null;
		$this->maxvolbytes = (!empty($data['maxvolbytes'])) ? $data['maxvolbytes'] : null;
		$this->inchanger = (!empty($data['inchanger'])) ? $data['inchanger'] : null;
		$this->storageid = (!empty($data['storageid'])) ? $data['storageid'] : null;
		$this->deviceid = (!empty($data['deviceid'])) ? $data['deviceid'] : null;
		$this->mediaaddressing = (!empty($data['mediaaddressing'])) ? $data['mediaaddressing'] : null;
		$this->volreadtime = (!empty($data['volreadtime'])) ? $data['volreadtime'] : null;
		$this->volwritetime = (!empty($data['volwritetime'])) ? $data['volwritetime'] : null;
		$this->endfile = (!empty($data['endfile'])) ? $data['endfile'] : null;
		$this->endblock = (!empty($data['endblock'])) ? $data['endblock'] : null;
		$this->locationid = (!empty($data['locationid'])) ? $data['locationid'] : null;
		$this->recyclecount = (!empty($data['recyclecount'])) ? $data['recyclecount'] : null;
		$this->initialwrite = (!empty($data['initialwrite'])) ? $data['initialwrite'] : null;
		$this->scratchpoolid = (!empty($data['scratchpoolid'])) ? $data['scratchpoolid'] : null;
		$this->recyclepoolid = (!empty($data['recyclepoolid'])) ? $data['recyclepoolid'] : null;
		$this->encryptionkey = (!empty($data['encryptionkey'])) ? $data['encryptionkey'] : null;
		$this->comment = (!empty($data['comment'])) ? $data['comment'] : null;
	}

}

