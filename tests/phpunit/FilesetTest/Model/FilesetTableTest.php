<?php

/**
 *
 * bareos-webui - Bareos Web-Frontend
 *
 * @link      https://github.com/bareos/bareos-webui for the canonical source repository
 * @copyright Copyright (c) 2013-2014 Bareos GmbH & Co. KG (http://www.bareos.org/)
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

namespace FilesetTest\Model;

use Fileset\Model\FilesetTable;
use Fileset\Model\Fileset;
use Zend\Db\ResultSet\ResultSet;
use PHPUnit_Framework_TestCase;

class FilesetTableTest extends PHPUnit_Framework_TestCase
{

   public function testFetchAllReturnsAllFilesets()
   {
      $resultSet = new ResultSet();

      $mockTableGateway = $this->getMock(
         'Zend\Db\TableGateway\TableGateway',
         array('select'),
         array(),
         '',
         false
      );

      $mockTableGateway->expects($this->once())
         ->method('select')
         ->with()
         ->will($this->returnValue($resultSet));

      $filesetTable = new FilesetTable($mockTableGateway);

      $this->assertSame($resultSet, $filesetTable->fetchAll());

   }

}
