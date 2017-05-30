<?php

/**
 *
 * bareos-webui - Bareos Web-Frontend
 *
 * @link      https://github.com/bareos/bareos-webui for the canonical source repository
 * @copyright Copyright (c) 2013-2017 Bareos GmbH & Co. KG (http://www.bareos.org/)
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

class JobModel
{
   /**
    * Get mulitple Jobs
    *
    * @param $bsock
    * @param $jobname
    * @param $days
    *
    * @return array
    */
   public function getJobs(&$bsock=null, $jobname=null, $days=null)
   {
      if(isset($bsock)) {
         if($days == "all") {
            if($jobname == "all") {
               $cmd = 'llist jobs';
            }
            else {
               $cmd = 'llist jobs jobname="'.$jobname.'"';
            }
         }
         else  {
            if($jobname == "all") {
               $cmd = 'llist jobs days='.$days;
            }
            else {
               $cmd = 'llist jobs jobname="'.$jobname.'" days='.$days;
            }
         }
         $result = $bsock->send_command($cmd, 2, null);
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
      else {
         throw new \Exception('Missing argument.');
      }
   }

   /**
    * Get Job by Status
    *
    * @param $bsock
    * @param $jobname
    * @param $status
    * @param $days
    * @param $hours
    *
    * @return array
    */
   public function getJobsByStatus(&$bsock=null, $jobname=null, $status=null, $days=null, $hours=null)
   {
      if(isset($bsock, $status)) {
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
         if($jobname != "all") {
            $cmd .= ' jobname="'.$jobname.'"';
         }
         $result = $bsock->send_command($cmd, 2, null);
         $jobs = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
         return array_reverse($jobs['result']['jobs']);
      }
      else {
         throw new \Exception('Missing argument.');
      }
   }

   /**
    * Get a single Job
    *
    * @param $bsock
    * @param $id
    *
    * @return array
    */
   public function getJob(&$bsock=null, $id=null)
   {
      if(isset($bsock, $id)) {
         $cmd = 'llist jobid='.$id.'';
         $result = $bsock->send_command($cmd, 2, null);
         $job = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
         return $job['result']['jobs'];
      }
      else {
         throw new \Exception('Missing argument.');
      }
   }

   /**
    * Get Job Log
    *
    * @param $bsock
    * @param $id
    *
    * @return array
    */
   public function getJobLog(&$bsock=null, $id=null)
   {
      if(isset($bsock, $id)) {
         $cmd = 'list joblog jobid='.$id.'';
         $result = $bsock->send_command($cmd, 2, null);
         if(preg_match('/Failed to send result as json. Maybe result message to long?/', $result)) {
            //return false;
            $error = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
            return $error['result']['error'];
         }
         else {
            $log = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
            return $log['result']['joblog'];
         }

      }
      else {
         throw new \Exception('Missing argument.');
      }
   }

   /**
    * Get Job Media
    *
    * @param $bsock
    * @param $jobid
    *
    * @return array
    */
   public function getJobMedia(&$bsock=null, $jobid=null)
   {
      $cmd = 'llist jobmedia jobid='.$jobid;
      $result = $bsock->send_command($cmd, 2, null);
      if(preg_match('/Failed to send result as json. Maybe result message to long?/', $result)) {
         //return false;
         $error = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
         return $error['result']['error'];
      }
      else {
         $jobmedia = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
         return $jobmedia['result']['jobmedia'];
      }
   }

   /**
    * Get Jobs by type
    *
    * @param $bsock
    * @param $type
    *
    * @return array
    */
   public function getJobsByType(&$bsock=null, $type=null)
   {
      if(isset($bsock)) {
         if($type == null) {
            $cmd = '.jobs';
         }
         else {
            $cmd = '.jobs type="'.$type.'"';
         }
         $result = $bsock->send_command($cmd, 2, null);
         $jobs = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
         return $jobs['result']['jobs'];
      }
      else {
         throw new \Exception('Missing argument.');
      }
   }

   /**
    * Get JobsLastStatus
    *
    * @param $bsock
    *
    * @return array
    */
   public function getJobsLastStatus(&$bsock=null)
   {
      if(isset($bsock)) {
         $cmd = 'llist jobs last current enabled';
         $result = $bsock->send_command($cmd, 2, null);
         $jobs = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
         return $jobs['result']['jobs'];
      }
      else {
         throw new \Exception('Missing argument.');
      }
   }

   /**
    * Get JobTotals
    *
    * @param $bsock
    *
    * @return array
    */
   public function getJobTotals(&$bsock=null)
   {
      if(isset($bsock)) {
         $cmd = 'list jobtotals';
         $result = $bsock->send_command($cmd, 2, null);
         $jobtotals = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
         return array(0 => $jobtotals['result']['jobtotals']);
      }
      else {
         throw new \Exception('Missing argument.');
      }
   }

   /**
    * Get Running Jobs Statistics
    *
    * @param $bsock
    *
    * @return array
    */
   public function getRunningJobsStatistics(&$bsock = null) {
      if(isset($bsock)) {

         $jobstats = [];
         $i = 0;

         // GET RUNNING JOBS
         $runningJobs = $this->getJobsByStatus($bsock, null, 'R');

         // COLLECT REQUIRED DATA FOR EACH RUNNING JOB
         foreach($runningJobs as $job) {

            // GET THE JOB STATS
            $cmd = 'list jobstatistics jobid=' . $job['jobid'];
            $result = $bsock->send_command($cmd, 2, null);
            $tmp = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);

            // JOBID, JOBNAME AND CLIENT
            $jobstats[$i]['jobid'] = $job['jobid'];
            $jobstats[$i]['name'] = $job['name'];
            $jobstats[$i]['client'] = $job['client'];
            $jobstats[$i]['level'] = $job['level'];

            if(count($tmp['result']['jobstats']) > 2) {

               // CALCULATE THE CURRENT TRANSFER SPEED OF THE INTERVAL
               $a = strtotime( $tmp['result']['jobstats'][count($tmp['result']['jobstats']) - 1]['sampletime'] );
               $b = strtotime( $tmp['result']['jobstats'][count($tmp['result']['jobstats']) - 2]['sampletime'] );
               $interval = $a - $b;

               if($interval > 0) {
                  $speed = ($tmp['result']['jobstats'][count($tmp['result']['jobstats']) - 1]['jobbytes'] - $tmp['result']['jobstats'][count($tmp['result']['jobstats']) - 2]['jobbytes']) / $interval;
                  $speed = round($speed, 2);
               }
               else {
                  $speed = 0;
               }

               $jobstats[$i]['speed'] = $speed;

               // JOBFILES
               $tmp = $tmp['result']['jobstats'][count($tmp['result']['jobstats']) - 1];
               if($tmp['jobfiles'] == null) {
                  $jobstats[$i]['jobfiles'] = 0;
               }
               else {
                  $jobstats[$i]['jobfiles'] = $tmp['jobfiles'];
               }

               // JOBBYTES
               $jobstats[$i]['jobbytes'] = $tmp['jobbytes'];

               // SAMPLETIME
               $jobstats[$i]['sampletime'] = $tmp['sampletime'];

               // LAST BACKUP SIZE
               $level = $jobstats[$i]['level'];
               $cmd = 'list jobs jobname=' . $job['name'] . ' client=' . $job['client'] . ' jobstatus=T joblevel=' . $level . ' last';

               $result = $bsock->send_command($cmd, 2, null);
               $tmp = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
               $jobstats[$i]['lastbackupsize'] = $tmp['result']['jobs'][0]['jobbytes'];
               if($jobstats[$i]['lastbackupsize'] > 0) {
                  if($jobstats[$i]['jobbytes'] > 0 && $tmp['result']['jobs'][0]['jobbytes'] > 0) {
                     $jobstats[$i]['progress'] = ceil( (($jobstats[$i]['jobbytes'] * 100) / $tmp['result']['jobs'][0]['jobbytes']));
                     if($jobstats[$i]['progress'] > 100) {
                        $jobstats[$i]['progress'] = 99;
                     }
                  }
                  else {
                     $jobstats[$i]['progress'] = 0;
                  }
               }
               else {
                  $jobstats[$i]['progress'] = 0;
               }
            }
            else {
               $jobstats[$i]['speed'] = 0;
               $jobstats[$i]['jobfiles'] = 0;
               $jobstats[$i]['jobbytes'] = 0;
               $jobstats[$i]['sampletime'] = null;
               $jobstats[$i]['progress'] = 0;
            }

            $i++;

         }
         return $jobstats;
      }
      else {
         throw new \Exception('Missing argument.');
      }
   }

   /**
    * Get the available Restore Jobs
    *
    * @param $bsock
    *
    * @return array
    */
   public function getRestoreJobs(&$bsock=null)
   {
      if(isset($bsock)) {
         $cmd = '.jobs type=R';
         $result = $bsock->send_command($cmd, 2, null);
         $restorejobs = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
         return $restorejobs['result']['jobs'];
      }
      else {
         throw new \Exception('Missing argument.');
      }
   }

   /**
    * Run a job as scheduled
    *
    * @param $bsock
    * @param $name
    *
    * @return string
    */
   public function runJob(&$bsock=null, $name=null)
   {
      if(isset($bsock, $name)) {
         $cmd = 'run job="'.$name.'" yes';
         $result = $bsock->send_command($cmd, 0, null);
         return $result;
      }
      else {
         throw new \Exception('Missing argument.');
      }
   }

   /**
    * Run a custom job
    *
    * @param $bsock
    * @param $jobname
    * @param $client
    * @param $fileset
    * @param $storage
    * @param $pool
    * @param $level
    * @param $priority
    * @param $backupformat
    * @param $when
    *
    * @return string
    */
   public function runCustomJob(&$bsock=null, $jobname=null, $client=null, $fileset=null, $storage=null, $pool=null, $level=null, $priority=null, $backupformat=null, $when=null)
   {
      if(isset($bsock, $jobname)) {
         $cmd = 'run job="' . $jobname . '"';
         if(!empty($client)) {
            $cmd .= ' client="' . $client . '"';
         }
         if(!empty($fileset)) {
            $cmd .= ' fileset="' . $fileset . '"';
         }
         if(!empty($storage)) {
            $cmd .= ' storage="' . $storage . '"';
         }
         if(!empty($pool)) {
            $cmd .= ' pool="' . $pool . '"';
         }
         if(!empty($level)) {
            $cmd .= ' level="' . $level . '"';
         }
         if(!empty($priority)) {
            $cmd .= ' priority="' . $priority . '"';
         }
         if(!empty($backupformat)) {
            $cmd .= ' backupformat="' . $backupformat . '"';
         }
         if(!empty($when)) {
            $cmd .= ' when="' . $when . '"';
         }
         $cmd .= ' yes';
         $result = $bsock->send_command($cmd, 0 , null);
         return 'Command send: '. $cmd . ' | Director message: ' . $result;
      }
      else {
         throw new \Exception('Missing argument.');
      }
   }

   /**
    * Re-Run a job
    *
    * @param $bsock
    * @param $id
    *
    * @return string
    */
   public function rerunJob(&$bsock=null, $id=null)
   {
      if(isset($bsock, $id)) {
         $cmd = 'rerun jobid='.$id.' yes';
         $result = $bsock->send_command($cmd, 0, null);
         return $result;
      }
      else {
         throw new \Exception('Missing argument.');
      }
   }

   /**
    * Cancel a job
    *
    * @param $bsock
    * @param $id
    *
    * @return string
    */
   public function cancelJob(&$bsock=null, $id=null)
   {
      if(isset($bsock, $id)) {
         $cmd = 'cancel jobid='.$id.' yes';
         $result = $bsock->send_command($cmd, 0, null);
         return $result;
      }
      else {
         throw new \Exception('Missing argument.');
      }
   }

   /**
    * Enable a job
    *
    * @param $bsock
    * @param $name
    *
    * @return string
    */
   public function enableJob(&$bsock=null, $name=null)
   {
      if(isset($bsock, $name)) {
         $cmd = 'enable job="'.$name.'" yes';
         $result = $bsock->send_command($cmd, 0, null);
         return $result;
      }
      else {
         throw new \Exception('Missing argument.');
      }
   }

   /**
    * Disable a job
    *
    * @param $bsock
    * @param $name
    *
    * @return string
    */
   public function disableJob(&$bsock=null, $name=null)
   {
      if(isset($bsock, $name)) {
         $cmd = 'disable job="'.$name.'" yes';
         $result = $bsock->send_command($cmd, 0, null);
         return $result;
      }
      else {
         throw new \Exception('Missing argument.');
      }
   }
}
