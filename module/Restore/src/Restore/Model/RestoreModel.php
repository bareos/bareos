<?php

/**
 *
 * bareos-webui - Bareos Web-Frontend
 *
 * @link      https://github.com/bareos/bareos-webui for the canonical source repository
 * @copyright Copyright (c) 2013-2015 Bareos GmbH & Co. KG (http://www.bareos.org/)
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

namespace Restore\Model;

use Zend\ServiceManager\ServiceLocatorAwareInterface;
use Zend\ServiceManager\ServiceLocatorInterface;

class RestoreModel implements ServiceLocatorAwareInterface
{
   protected $serviceLocator;
   protected $director;

   public function __construct()
   {
   }

   public function setServiceLocator(ServiceLocatorInterface $serviceLocator)
   {
      $this->serviceLocator = $serviceLocator;
   }

   public function getServiceLocator()
   {
      return $this->serviceLocator;
   }

   public function getDirectories($jobid=null, $pathid=null) {
      if(isset($jobid)) {
         if($pathid == null || $pathid== "#") {
            $cmd = '.bvfs_lsdirs jobid='.$jobid.' path=';
         }
         else {
            $cmd = '.bvfs_lsdirs jobid='.$jobid.' pathid='.abs($pathid).'';
         }
         $this->director = $this->getServiceLocator()->get('director');
         $result = $this->director->send_command($cmd, 2, $jobid);
         $directories = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
         if(empty($directories['result']['directories'])) {
            $cmd = '.bvfs_lsdirs jobid='.$jobid.' path=@';
            $result = $this->director->send_command($cmd, 2, $jobid);
            $directories = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
            if(empty($directories['result']['directories'])) {
               return null;
            }
            else {
               return $directories['result']['directories'];
            }
         }
         else {
            return $directories['result']['directories'];
         }
      }
      else {
         return false;
      }
   }

   public function getFiles($jobid=null, $pathid=null) {
      if(isset($jobid)) {
         if($pathid == null || $pathid == "#") {
            $cmd = '.bvfs_lsfiles jobid='.$jobid.' path=';
         }
         else {
            $cmd = '.bvfs_lsfiles jobid='.$jobid.' pathid='.abs($pathid).'';
         }
         $this->director = $this->getServiceLocator()->get('director');
         $result = $this->director->send_command($cmd, 2, $jobid);
         $files = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
         if(empty($files['result']['files'])) {
            $cmd = '.bvfs_lsfiles jobid='.$jobid.' path=@';
            $result = $this->director->send_command($cmd, 2, $jobid);
            $files = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
            if(empty($files['result']['files'])) {
               return null;
            }
            else {
               return $files['result']['files'];
            }
         }
         else {
            return $files['result']['files'];
         }
      }
      else {
         return false;
      }
   }

   public function getJobs()
   {
      $cmd = 'llist jobs';
      $this->director = $this->getServiceLocator()->get('director');
      $result = $this->director->send_command($cmd, 2, null);
      $jobs = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
      if(empty($jobs['result']['jobs'])) {
         return null;
      }
      else {
         return $jobs['result']['jobs'];
      }
   }

   public function getClients()
   {
      $cmd = 'llist clients';
      $this->director = $this->getServiceLocator()->get('director');
      $result = $this->director->send_command($cmd, 2, null);
      $clients = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
      return $clients['result']['clients'];
   }

   public function getFilesets()
   {
      $cmd = '.filesets';
      $this->director = $this->getServiceLocator()->get('director');
      $result = $this->director->send_command($cmd, 2, null);
      $filesets = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
      return $filesets['result']['filesets'];
   }

   public function getClientBackups($client=null, $fileset=null, $order=null, $limit=null)
   {
      if(isset($client, $fileset, $order)) {
         if($limit != null) {
            $cmd = 'list backups client='.$client.' fileset='.$fileset.' order='.$order.' limit='.$limit.'';
         }
         else {
            $cmd = 'list backups client='.$client.' fileset='.$fileset.' order='.$order.'';
         }
         $this->director = $this->getServiceLocator()->get('director');
         $result = $this->director->send_command($cmd, 2, null);
         $backups = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
         if(empty($backups['result'])) {
            return null;
         }
         else {
            return $backups['result']['backups'];
         }
      }
      else {
         return false;
      }
   }

   public function getRestoreJobs()
   {
      $cmd = '.jobs type=R';
      $this->director = $this->getServiceLocator()->get('director');
      $result = $this->director->send_command($cmd, 2, null);
      $restorejobs = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
      return $restorejobs['result']['jobs'];
   }

   public function getJobIds($jobid=null, $mergefilesets=0, $mergejobs=0)
   {
      if(isset($jobid)) {
         if($mergefilesets == 0 && $mergejobs == 0) {
            $cmd = '.bvfs_get_jobids jobid='.$jobid.' all';
         }
         elseif($mergefilesets == 1 && $mergejobs == 0) {
            $cmd = '.bvfs_get_jobids jobid='.$jobid.'';
         }
         elseif($mergefilesets == 1 && $mergejobs == 1) {
            return $jobid;
         }
         $this->director = $this->getServiceLocator()->get('director');
         $result = $this->director->send_command($cmd, 2, null);
         $jobids = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
         if(!empty($jobids['result'])) {
            $result = "";
            $i = count($jobids['result']['jobids']);
            foreach($jobids['result']['jobids'] as $jobid) {
               $result .= $jobid['id'];
               --$i;
               if($i > 0) {
                  $result .= ",";
               }
            }
         }
         return $result;
      }
      else {
         return false;
      }
   }

   public function restore($type=null, $jobid=null, $client=null, $restoreclient=null, $restorejob=null, $where=null, $fileid=null, $dirid=null, $jobids=null, $replace=null)
   {
      if(isset($type)) {
         $this->director = $this->getServiceLocator()->get('director');
         if($type == "client") {
            $result = $this->director->restore($type, $jobid, $client, $restoreclient, $restorejob, $where, $fileid, $dirid, $jobids, $replace);
         }
         elseif($type == "job") {
            // TODO
         }
         return $result;
      }
      else {
         return false;
      }
   }
}
