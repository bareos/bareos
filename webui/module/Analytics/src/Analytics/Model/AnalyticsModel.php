<?php

/**
 *
 * bareos-webui - Bareos Web-Frontend
 *
 * @link      https://github.com/bareos/bareos for the canonical source repository
 * @copyright Copyright (C) 2013-2026 Bareos GmbH & Co. KG (http://www.bareos.org/)
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
    public function getJobTotals(&$bsock = null): array
    {
        if (isset($bsock)) {
            $cmd = 'list jobtotals';
            $result = $bsock->send_command($cmd, 2);
            $jobtotals = \Laminas\Json\Json::decode($result, \Laminas\Json\Json::TYPE_ARRAY);
            $children = array("children" => $jobtotals['result']['jobs']);
            return $children;
        } else {
            throw new \Exception('Missing argument.');
        }
    }

    public function getOverallJobTotals(&$bsock = null): array
    {
        if (isset($bsock)) {
            $cmd = 'list jobtotals';
            $result = $bsock->send_command($cmd, 2);
            $jobtotals = \Laminas\Json\Json::decode($result, \Laminas\Json\Json::TYPE_ARRAY);
            $result = $jobtotals['result']['jobtotals'];
            return $result;
        } else {
            throw new \Exception('Missing argument.');
        }
    }

    public function getConfigResourceId($type, &$name): string
    {
        # valid CSS id can only contain characters [a-zA-Z0-9-_]
        return $type . "_" . md5($name);
    }

    public function createConfigResourceNode($type, &$resource): \stdClass
    {
        $node = new \stdClass();
        $node->type = $type;
        $node->id = $this->getConfigResourceId($type, $resource["name"]);
        $node->name = $resource["name"];
        $node->resource = $resource;
        return $node;
    }


    public function getConfigResourceGraph(&$bsock = null): array
    {
        if (!isset($bsock)) {
            throw new \Exception('Missing argument.');
        }

        $clients_cache = \Laminas\Json\Json::decode(
            $bsock->send_command('show clients', 2),
            \Laminas\Json\Json::TYPE_ARRAY
        )['result']['clients'];

        $jobs_cache = \Laminas\Json\Json::decode(
            $bsock->send_command('show jobs', 2),
            \Laminas\Json\Json::TYPE_ARRAY
        )['result']['jobs'];

        $jobdefs_cache = \Laminas\Json\Json::decode(
            $bsock->send_command('show jobdefs', 2),
            \Laminas\Json\Json::TYPE_ARRAY
        )['result']['jobdefs'];

        $filesets_cache = \Laminas\Json\Json::decode(
            $bsock->send_command('show fileset', 2),
            \Laminas\Json\Json::TYPE_ARRAY
        )['result']['filesets'];

        $schedules_cache = \Laminas\Json\Json::decode(
            $bsock->send_command('show schedules', 2),
            \Laminas\Json\Json::TYPE_ARRAY
        )['result']['schedules'];

        // Build O(n) lookup maps
        $jobs_by_client = [];
        foreach ($jobs_cache as $job) {
            if (isset($job['client'])) {
                $jobs_by_client[$job['client']][] = $job['name'];
            }
        }

        $jobdefs_by_name = [];
        foreach ($jobdefs_cache as $jd) {
            $jobdefs_by_name[$jd['name']] = true;
        }

        $filesets_by_name = [];
        foreach ($filesets_cache as $fs) {
            $filesets_by_name[$fs['name']] = true;
        }

        $schedules_by_name = [];
        foreach ($schedules_cache as $sc) {
            $schedules_by_name[$sc['name']] = true;
        }

        $config_graph = new \stdClass();
        $nodes = [];
        $links = [];

        foreach ($clients_cache as $client) {
            $node = $this->createConfigResourceNode("client", $client);
            $nodes[] = $node;
            foreach ($jobs_by_client[$client['name']] ?? [] as $job_name) {
                $link = new \stdClass();
                $link->source = $node->id;
                $link->target = $this->getConfigResourceId("job", $job_name);
                $links[] = $link;
            }
        }

        foreach ($jobs_cache as $job) {
            $node = $this->createConfigResourceNode("job", $job);
            $nodes[] = $node;
            if (isset($job['jobdefs']) && isset($jobdefs_by_name[$job['jobdefs']])) {
                $link = new \stdClass();
                $link->source = $node->id;
                $link->target = $this->getConfigResourceId("jobdefs", $job['jobdefs']);
                $links[] = $link;
            }
            if (isset($job['schedule']) && isset($schedules_by_name[$job['schedule']])) {
                $link = new \stdClass();
                $link->source = $node->id;
                $link->target = $this->getConfigResourceId("schedule", $job['schedule']);
                $links[] = $link;
            }
            if (isset($job['fileset']) && isset($filesets_by_name[$job['fileset']])) {
                $link = new \stdClass();
                $link->source = $node->id;
                $link->target = $this->getConfigResourceId("fileset", $job['fileset']);
                $links[] = $link;
            }
        }

        foreach ($filesets_cache as $fileset) {
            $nodes[] = $this->createConfigResourceNode("fileset", $fileset);
        }

        foreach ($jobdefs_cache as $jobdefs) {
            $node = $this->createConfigResourceNode("jobdefs", $jobdefs);
            $nodes[] = $node;
            if (isset($jobdefs['schedule']) && isset($schedules_by_name[$jobdefs['schedule']])) {
                $link = new \stdClass();
                $link->source = $node->id;
                $link->target = $this->getConfigResourceId("schedule", $jobdefs['schedule']);
                $links[] = $link;
            }
            if (isset($jobdefs['fileset']) && isset($filesets_by_name[$jobdefs['fileset']])) {
                $link = new \stdClass();
                $link->source = $node->id;
                $link->target = $this->getConfigResourceId("fileset", $jobdefs['fileset']);
                $links[] = $link;
            }
        }

        foreach ($schedules_cache as $schedule) {
            $nodes[] = $this->createConfigResourceNode("schedule", $schedule);
        }

        $config_graph->nodes = $nodes;
        $config_graph->links = $links;

        return $config_graph;
    }
}
