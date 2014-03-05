<?php

/**
 *
 * bareos-webui - Bareos Web-Frontend
 * 
 * @link      https://github.com/bareos/bareos-webui for the canonical source repository
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

namespace Pool\Model;

class Pool 
{

	public $poolid;
	public $name;
	public $numvols;
	public $maxvols;
	public $useonce;
	public $usecatalog;
	public $acceptanyvolume;
	public $volretention;
	public $voluseduration;
	public $maxvoljobs;
	public $maxvolfiles;
	public $maxvolbytes;
	public $autoprune;
	public $recycle;
	public $actiononpurge;
	public $pooltype;
	public $labeltype;
	public $labelformat;
	public $enabled;
	public $scratchpoolid;
	public $recyclepoolid;
	public $nextpoolid;
	public $migrationhighbytes;
	public $migrationlowbytes;
	public $migrationtime;

	public function exchangeArray($data)
	{
		$this->poolid = (!empty($data['poolid'])) ? $data['poolid'] : null;
		$this->name = (!empty($data['name'])) ? $data['name'] : null;
		$this->numvols = (!empty($data['numvols'])) ? $data['numvols'] : null;
		$this->maxvols = (!empty($data['maxvols'])) ? $data['maxvols'] : null;
		$this->useonce = (!empty($data['useonce'])) ? $data['useonce'] : null;
		$this->usecatalog = (!empty($data['usecatalog'])) ? $data['usecatalog'] : null;
		$this->acceptanyvolume = (!empty($data['acceptanyvolume'])) ? $data['acceptanyvolume'] : null;
		$this->volretention = (!empty($data['volretention'])) ? $data['volretention'] : null;
		$this->voluseduration = (!empty($data['voluseduration'])) ? $data['voluseduration'] : null;
		$this->maxvoljobs = (!empty($data['maxvoljobs'])) ? $data['maxvoljobs'] : null;
		$this->maxvolfiles = (!empty($data['maxvolfiles'])) ? $data['maxvolfiles'] : null;
		$this->maxvolbytes = (!empty($data['maxvolbytes'])) ? $data['maxvolbytes'] : null;
		$this->autoprune = (!empty($data['autoprune'])) ? $data['autoprune'] : null;
		$this->recycle = (!empty($data['recycle'])) ? $data['recycle'] : null;
		$this->actiononpurge = (!empty($data['actiononpurge'])) ? $data['actiononpurge'] : null;
		$this->pooltype = (!empty($data['pooltype'])) ? $data['pooltype'] : null;
		$this->labeltype = (!empty($data['labeltype'])) ? $data['labeltype'] : null;
		$this->labelformat = (!empty($data['labelformat'])) ? $data['labelformat'] : null;
		$this->enabled = (!empty($data['enabled'])) ? $data['enabled'] : null;
		$this->scratchpoolid = (!empty($data['scratchpoolid'])) ? $data['scratchpoolid'] : null;
		$this->recyclepoolid = (!empty($data['recyclepoolid'])) ? $data['recyclepoolid'] : null;
		$this->nextpoolid = (!empty($data['nextpoolid'])) ? $data['nextpoolid'] : null;
		$this->migrationhighbytes = (!empty($data['migrationhighbytes'])) ? $data['migrationhighbytes'] : null;
		$this->migrationlowbytes = (!empty($data['migrationlowbytes'])) ? $data['migrationlowbytes'] : null;
		$this->migrationtime = (!empty($data['migrationtime'])) ? $data['migrationtime'] : null;
	}

}

