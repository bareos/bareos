<?php

/**
 *
 * bareos-webui - Bareos Web-Frontend
 *
 * @link      https://github.com/bareos/bareos for the canonical source repository
 * @copyright Copyright (c) 2013-2023 Bareos GmbH & Co. KG (http://www.bareos.org/)
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

namespace Analytics\Model;

class AnalyticsModel
{
  public function getJobTotals(&$bsock=null)
   {
      if(isset($bsock)) {
         $cmd = 'list jobtotals';
         $result = $bsock->send_command($cmd, 2);
         $jobtotals = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
         $children = array("children" => $jobtotals['result']['jobs']);
         return $children;
      }
      else {
         throw new \Exception('Missing argument.');
      }
   }

   public function getOverallJobTotals(&$bsock=null)
   {
      if(isset($bsock)) {
         $cmd = 'list jobtotals';
         $result = $bsock->send_command($cmd, 2);
         $jobtotals = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
         $result = $jobtotals['result']['jobtotals'];
         return $result;
      }
      else {
         throw new \Exception('Missing argument.');
      }
   }

   public function getConfigTree(&$bsock=null)
   {
      if(isset($bsock)) {

         // note: has no result in api 2 on a restricted named console
         /*
         $cmd = 'show directors';
         $result = $bsock->send_command($cmd, 2);
         $directors = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
         $directors_cache = $directors['result']['directors'];
         */

         $cmd = 'show clients';
         $result = $bsock->send_command($cmd, 2);
         $clients = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
         $clients_cache = $clients['result']['clients'];

         $cmd = 'show jobs';
         $result = $bsock->send_command($cmd, 2);
         $jobs = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
         $jobs_cache = $jobs['result']['jobs'];

         $cmd = 'show jobdefs';
         $result = $bsock->send_command($cmd, 2);
         $jobdefs = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
         $jobdefs_cache = $jobdefs['result']['jobdefs'];

         /*
         $cmd = 'show fileset';
         $result = $bsock->send_command($cmd, 2);
         $filesets = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
         $filesets_cache = $filesets['result']['filesets'];
         */

         $config_tree = array('name' => 'DIR', 'children' => array());

         foreach ($clients_cache as $client_key=>$client) {

            $client_children = array();

            foreach ($jobs_cache as $job_key=>$job) {

               $job_children = array();

               if (isset($job["client"]) && $job["client"] == $client_key) {
                  if (isset($job["fileset"])) {
                     array_push($job_children, array('name' => $job['fileset']));
                  } else {
                     foreach ($jobdefs_cache as $jobdefs_key=>$jobdefs) {
                        if ($job["jobdefs"] == $jobdefs_key) {
                           array_push($job_children, array('name' => $jobdefs['fileset']));
                        }
                     }
                  }
                  array_push($client_children, array('name' => $job_key, 'children' => $job_children));
               }

               if (!isset($job['client'])) {
                  foreach ($jobdefs_cache as $jobdefs_key=>$jobdefs) {
                     if (isset($job['jobdefs']) && $job['jobdefs'] == $jobdefs_key && $client_key == $jobdefs['client']) {
                        if (isset($job['fileset'])) {
                           array_push($job_children, array('name' => $job['fileset']));
                        } else {
                           array_push($job_children, array('name' => $jobdefs['fileset']));
                        }
                        array_push($client_children, array('name' => $job_key, 'children' => $job_children));
                     }
                  }
               }
            }

            array_push($config_tree["children"], array("name" => $client_key, "children" => $client_children));
         }

         return $config_tree;
      }
      else {
         throw new \Exception('Missing argument.');
      }
   }
}
