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

namespace Job\Model;

use Zend\ServiceManager\ServiceLocatorAwareInterface;
use Zend\ServiceManager\ServiceLocatorInterface;

class JobModel implements ServiceLocatorAwareInterface
{
   protected $serviceLocator;
   protected $director;

   public function __constructor()
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

   public function getJobs($status=null, $period=null)
   {
      if($period == "all") {
         $cmd = 'llist jobs';
      }
      else {
         $cmd = 'llist jobs days='.$period;
      }

      $this->director = $this->getServiceLocator()->get('director');
      $result = $this->director->send_command($cmd, 2, null);
      if(preg_match('/Failed to send result as json. Maybe result message to long?/', $result)) {
         //return false;
         $error = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
         return $error['result']['error'];
      }
      else {
         $jobs = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
         return $jobs['result']['jobs'];
      }
   }

   public function getJobsByStatus($status=null, $days=null, $hours=null)
   {
      if(isset($status)) {
         if(isset($days)) {
            if($days == "all") {
               $cmd = 'llist jobs jobstatus='.$status.'';
            }
            else {
               $cmd = 'llist jobs jobstatus='.$status.' days='.$days.'';
            }
         }
         elseif(isset($hours)) {
            if($hours == "all") {
               $cmd = 'llist jobs jobstatus='.$status.'';
            }
            else {
               $cmd = 'llist jobs jobstatus='.$status.' hours='.$hours.'';
            }
         }
         else {
            $cmd = 'llist jobs jobstatus='.$status.'';
         }
         $this->director = $this->getServiceLocator()->get('director');
         $result = $this->director->send_command($cmd, 2, null);
         $jobs = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
         return array_reverse($jobs['result']['jobs']);
      }
      else {
         return false;
      }
   }

   public function getJob($id=null)
   {
      if(isset($id)) {
         $cmd = 'llist jobid='.$id.'';
         $this->director = $this->getServiceLocator()->get('director');
         $result = $this->director->send_command($cmd, 2, null);
         $job = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
         return $job['result']['jobs'];
      }
      else {
         return false;
      }
   }

   public function getJobLog($id=null)
   {
      if(isset($id)) {
         $cmd = 'list joblog jobid='.$id.'';
         $this->director = $this->getServiceLocator()->get('director');
         $result = $this->director->send_command($cmd, 2, null);
         $log = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
         return $log['result']['joblog'];
      }
      else {
         return false;
      }
   }

   public function getBackupJobs()
   {
      $cmd = '.jobs type=B';
      $this->director = $this->getServiceLocator()->get('director');
      $result = $this->director->send_command($cmd, 2, null);
      $jobs = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
      return $jobs['result']['jobs'];
   }

   public function runJob($name=null)
   {
      if(isset($name)) {
         $cmd = 'run job="'.$name.'" yes';
         $this->director = $this->getServiceLocator()->get('director');
         $result = $this->director->send_command($cmd, 0, null);
         return $result;
      }
      else {
         return false;
      }
   }

   public function rerunJob($id=null)
   {
      if(isset($id)) {
         $cmd = 'rerun jobid='.$id.' yes';
         $this->director = $this->getServiceLocator()->get('director');
         $result = $this->director->send_command($cmd, 0, null);
         return $result;
      }
      else {
         return false;
      }
   }

   public function cancelJob($id=null)
   {
      if(isset($id)) {
         $cmd = 'cancel jobid='.$id.' yes';
         $this->director = $this->getServiceLocator()->get('director');
         $result = $this->director->send_command($cmd, 0, null);
         return $result;
      }
      else {
         return false;
      }
   }

   public function enableJob($name=null)
   {
      if(isset($name)) {
         $cmd = 'enable job="'.$name.'" yes';
         $this->director = $this->getServiceLocator()->get('director');
         $result = $this->director->send_command($cmd, 0, null);
         return $result;
      }
      else {
         return false;
      }
   }

   public function disableJob($name=null)
   {
      if(isset($name)) {
         $cmd = 'disable job="'.$name.'" yes';
         $this->director = $this->getServiceLocator()->get('director');
         $result = $this->director->send_command($cmd, 0, null);
         return $result;
      }
      else {
         return false;
      }
   }
}
