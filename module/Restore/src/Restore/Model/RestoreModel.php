<?php

/**
 *
 * bareos-webui - Bareos Web-Frontend
 *
 * @link      https://github.com/bareos/bareos-webui for the canonical source repository
 * @copyright Copyright (c) 2013-2016 Bareos GmbH & Co. KG (http://www.bareos.org/)
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

class RestoreModel
{

   public function getDirectories(&$bsock=null, $jobid=null, $pathid=null) {
      if(isset($bsock)) {
         if($pathid == null || $pathid== "#") {
            $cmd = '.bvfs_lsdirs jobid='.$jobid.' path=';
         }
         else {
            $cmd = '.bvfs_lsdirs jobid='.$jobid.' pathid='.abs($pathid).'';
         }
         $result = $bsock->send_command($cmd, 2, $jobid);
         $directories = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
         if(empty($directories['result']['directories'])) {
            $cmd = '.bvfs_lsdirs jobid='.$jobid.' path=@';
            $result = $bsock->send_command($cmd, 2, $jobid);
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
         throw new \Exception('Missing argument.');
      }
   }

   public function getFiles(&$bsock=null, $jobid=null, $pathid=null) {
      if(isset($bsock)) {
         if($pathid == null || $pathid == "#") {
            $cmd = '.bvfs_lsfiles jobid='.$jobid.' path=';
         }
         else {
            $cmd = '.bvfs_lsfiles jobid='.$jobid.' pathid='.abs($pathid).'';
         }
         $result = $bsock->send_command($cmd, 2, $jobid);
         $files = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
         if(empty($files['result']['files'])) {
            $cmd = '.bvfs_lsfiles jobid='.$jobid.' path=@';
            $result = $bsock->send_command($cmd, 2, $jobid);
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
         throw new \Exception('Missing argument.');
      }
   }

   public function getJobIds(&$bsock=null, $jobid=null, $mergefilesets=0, $mergejobs=0)
   {
      if(isset($bsock)) {
         if($mergefilesets == 1 && $mergejobs == 1) {
            return $jobid;
         }
         if($mergefilesets == 0) {
            $cmd = '.bvfs_get_jobids jobid='.$jobid.' all';
         }
         else {
            $cmd = '.bvfs_get_jobids jobid='.$jobid.'';
         }
         $result = $bsock->send_command($cmd, 2, null);
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
         throw new \Exception('Missing argument.');
      }
   }

   public function restore(&$bsock=null, $type=null, $jobid=null, $client=null, $restoreclient=null, $restorejob=null, $where=null, $fileid=null, $dirid=null, $jobids=null, $replace=null)
   {
      if(isset($bsock, $type)) {
         if($type == "client") {
            $result = $bsock->restore($type, $jobid, $client, $restoreclient, $restorejob, $where, $fileid, $dirid, $jobids, $replace);
         }
         elseif($type == "job") {
            // TODO
         }
         return $result;
      }
      else {
         throw new \Exception('Missing argument');
      }
   }
}
