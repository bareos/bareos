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

namespace Client\Model;

class ClientModel
{
    /**
     * Get all Clients by llist clients command
     *
     * @param $bsock
     *
     * @return array
     */
    public function getClients(&$bsock = null): array
    {
        if (!isset($bsock)) {
            throw new \Exception('Missing argument.');
        }

        $result = $bsock->call_json("llist clients current");
        return $result["clients"];
    }

    /**
     * Get all Clients by .clients command
     *
     * @param $bsock
     *
     * @return array
     */
    public function getDotClients(&$bsock = null): array
    {
        if (!isset($bsock)) {
            throw new \Exception('Missing argument.');
        }

        $result = $bsock->call_json(".clients");
        return $result["clients"];
    }

    /**
     * Get all known clients.
     *
     * @param $bsock
     *
     * @return array
     */
    public function getClientsWithBackups(&$bsock = null): array
    {
        if (!isset($bsock)) {
            throw new \Exception('Missing argument.');
        }

        $result = $bsock->call_json("list clients");
        return $result["clients"];
    }

    /**
     * Get a single Client by llist client command
     *
     * @param $bsock
     * @param $client
     *
     * @return array
     */
    public function getClient($client, &$bsock = null): array
    {
        if (!isset($bsock, $client)) {
            throw new \Exception('Missing argument.');
        }

        $cmd = 'llist client="' . $client . '"';
        $result = $bsock->call_json($cmd);
        return $result["clients"];
    }

    /**
     * Get Client Backups by llist backups command
     *
     * @param $bsock
     * @param $client
     * @param $fileset
     * @param $order
     * @param $limit
     *
     * @return array
     */
    public function getClientBackups(&$bsock = null, $client = null, $fileset = null, $order = null, $limit = null): array
    {
        if (isset($bsock, $client)) {
            $cmd = 'llist backups client="' . $client . '"';
            if ($fileset != null) {
                $cmd .= ' fileset="' . $fileset . '"';
            }
            if ($order != null) {
                $cmd .= ' order=' . $order;
            }
            if ($limit != null) {
                $cmd .= ' limit=' . $limit;
            }
            $result = $bsock->send_command($cmd, 2);
            $backups = \Laminas\Json\Json::decode($result, \Laminas\Json\Json::TYPE_ARRAY);

            if (!isset($limit)) {
                $all_filesets_result = \Laminas\Json\Json::decode(
                    $bsock->send_command('show fileset', 2),
                    \Laminas\Json\Json::TYPE_ARRAY
                );
                $filesets_all = $all_filesets_result['result']['filesets'] ?? [];

                $fileset_plugin_map = [];
                foreach ($filesets_all as $fs_name => $fs) {
                    $fileset_plugin_map[$fs_name] = !empty($fs['include'][0]['plugin']);
                }

                foreach ($backups['result']['backups'] as $key => $backup) {
                    if (
                        isset($backup['fileset'])
                        && array_key_exists($backup['fileset'], $fileset_plugin_map)
                    ) {
                        $backups['result']['backups'][$key]['pluginjob']
                            = $fileset_plugin_map[$backup['fileset']];
                    } elseif (isset($backup['fileset'])) {
                        error_log(
                            'getClientBackups: missing fileset metadata for "'
                            . $backup['fileset']
                            . '"'
                        );
                    }
                }
            }
            return $backups['result']['backups'];
        } else {
            throw new \Exception('Missing argument.');
        }
    }

    /**
     * Get Client Jobs by llist jobs command
     *
     * @param $bsock
     * @param $client
     * @param $fileset
     * @param $order
     * @param $limit
     *
     * @return array
     */
    public function getClientJobs(&$bsock = null, $client = null, $fileset = null, $order = null, $limit = null): array
    {
        if (isset($bsock, $client)) {
            $cmd = 'llist jobs client="' . $client . '"';
            if ($fileset != null) {
                $cmd .= ' fileset="' . $fileset . '"';
            }
            if ($order != null) {
                $cmd .= ' order=' . $order;
            }
            if ($limit != null) {
                $cmd .= ' limit=' . $limit;
            }
            $result = $bsock->send_command($cmd, 2);
            $backups = \Laminas\Json\Json::decode($result, \Laminas\Json\Json::TYPE_ARRAY);
            return $backups['result']['jobs'];
        } else {
            throw new \Exception('Missing argument.');
        }
    }

    /**
     * Get the status of a single Client by status client command
     *
     * @param $bsock
     * @param $name
     *
     * @return string
     */
    public function statusClient(&$bsock = null, $name = null): string
    {
        if (isset($bsock, $name)) {
            $cmd = 'status client="' . $name . '"';
            $result = $bsock->send_command($cmd, 0);
            return $result;
        } else {
            throw new \Exception('Missing argument.');
        }
    }

    /**
     * Enable a single Client by enable command
     *
     * @param $bsock
     * @param $name
     *
     * @return string
     */
    public function enableClient(&$bsock = null, $name = null): string
    {
        if (isset($bsock, $name)) {
            $cmd = 'enable client="' . $name . '" yes';
            $result = $bsock->send_command($cmd, 0);
            return $result;
        } else {
            throw new \Exception('Missing argument.');
        }
    }

    /**
     * Disable a single Client by disable command
     *
     * @param $bsock
     * @param $name
     *
     * @return string
     */
    public function disableClient(&$bsock = null, $name = null): string
    {
        if (isset($bsock, $name)) {
            $cmd = 'disable client="' . $name . '" yes';
            $result = $bsock->send_command($cmd, 0);
            return $result;
        } else {
            throw new \Exception('Missing argument.');
        }
    }
}
