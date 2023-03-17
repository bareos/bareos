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
    public function getJobTotals(&$bsock = null)
    {
        if (isset($bsock)) {
            $cmd = 'list jobtotals';
            $result = $bsock->send_command($cmd, 2);
            $jobtotals = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
            $children = array("children" => $jobtotals['result']['jobs']);
            return $children;
        } else {
            throw new \Exception('Missing argument.');
        }
    }

    public function getOverallJobTotals(&$bsock = null)
    {
        if (isset($bsock)) {
            $cmd = 'list jobtotals';
            $result = $bsock->send_command($cmd, 2);
            $jobtotals = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
            $result = $jobtotals['result']['jobtotals'];
            return $result;
        } else {
            throw new \Exception('Missing argument.');
        }
    }

    public function getConfigResourceId($type, &$name)
    {
        # valid CSS id can only contain characters [a-zA-Z0-9-_]
        return $type . "_" . md5($name);
    }

    public function createConfigResourceNode($type, &$resource)
    {
        $node = new \stdClass();
        $node->type = $type;
        $node->id = $this->getConfigResourceId($type, $resource["name"]);
        $node->name = $resource["name"];
        $node->resource = $resource;
        return $node;
    }


    public function getConfigResourceGraph(&$bsock = null)
    {
        if (!isset($bsock)) {
            throw new \Exception('Missing argument.');
        }

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

        $cmd = 'show fileset';
        $result = $bsock->send_command($cmd, 2);
        $filesets = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
        $filesets_cache = $filesets['result']['filesets'];

        $cmd = 'show schedules';
        $result = $bsock->send_command($cmd, 2);
        $schedules = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
        $schedules_cache = $schedules['result']['schedules'];

        $config_graph = new \stdClass();
        $nodes = array();
        $links = array();

        foreach ($clients_cache as $client_key => $client) {
            $node = $this->createConfigResourceNode("client", $client);
            array_push($nodes, $node);
            foreach ($jobs_cache as $job_key => $job) {
                if (isset($job['client'])) {
                    if ($client["name"] === $job["client"]) {
                        $link = new \stdClass();
                        $link->source = $node->id;
                        $link->target = $this->getConfigResourceId("job", $job["name"]);
                        array_push($links, $link);
                    }
                }
            }
        }
        foreach ($jobs_cache as $job_key => $job) {
            $node = $this->createConfigResourceNode("job", $job);
            array_push($nodes, $node);
            if (isset($job["jobdefs"])) {
                foreach ($jobdefs_cache as $jobdefs_key => $jobdefs) {
                    if ($job["jobdefs"] === $jobdefs["name"]) {
                        $link = new \stdClass();
                        $link->source = $node->id;
                        $link->target = $this->getConfigResourceId("jobdefs", $jobdefs["name"]);
                        array_push($links, $link);
                    }
                }
            }
            if (isset($job["schedule"])) {
                foreach ($schedules_cache as $schedule_key => $schedule) {
                    if ($job["schedule"] === $schedule["name"]) {
                        $link = new \stdClass();
                        $link->source = $node->id;
                        $link->target = $this->getConfigResourceId("schedule", $schedule["name"]);
                        array_push($links, $link);
                    }
                }
            }
            if (isset($job["fileset"])) {
                foreach ($filesets_cache as $filesets_key => $fileset) {
                    if ($job["fileset"] === $fileset["name"]) {
                        $link = new \stdClass();
                        $link->source = $node->id;
                        $link->target = $this->getConfigResourceId("fileset", $fileset["name"]);
                        array_push($links, $link);
                    }
                }
            }
        }

        foreach ($filesets_cache as $fileset_key => $fileset) {
            $node = $this->createConfigResourceNode("fileset", $fileset);
            array_push($nodes, $node);
        }

        foreach ($jobdefs_cache as $jobdefs_key => $jobdefs) {
            $node = $this->createConfigResourceNode("jobdefs", $jobdefs);
            array_push($nodes, $node);
            if (isset($jobdefs["schedule"])) {
                foreach ($schedules_cache as $schedule_key => $schedule) {
                    if ($jobdefs["schedule"] === $schedule["name"]) {
                        $link = new \stdClass();
                        $link->source = $node->id;
                        $link->target = $this->getConfigResourceId("schedule", $schedule["name"]);
                        array_push($links, $link);
                    }
                }
            }
            if (isset($jobdefs["fileset"])) {
                foreach ($filesets_cache as $fileset_key => $fileset) {
                    if ($jobdefs["fileset"] === $fileset["name"]) {
                        $link = new \stdClass();
                        $link->source = $node->id;
                        $link->target = $this->getConfigResourceId("fileset", $fileset["name"]);
                        array_push($links, $link);
                    }
                }
            }
        }

        foreach ($schedules_cache as $schedule_key => $schedule) {
            $node = $this->createConfigResourceNode("schedule", $schedule);
            array_push($nodes, $node);
        }

        $config_graph->nodes = $nodes;
        $config_graph->links = $links;

        return $config_graph;
    }
}
