<?php

/**
 *
 * bareos-webui - Bareos Web-Frontend
 *
 * @link      https://github.com/bareos/bareos for the canonical source repository
 * @copyright Copyright (C) 2013-2025 Bareos GmbH & Co. KG (http://www.bareos.org/)
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

namespace Job\Form;

use Laminas\Form\Form;
use Laminas\Form\Element;

class RunJobForm extends Form
{
    protected $clients;
    protected $jobs;
    protected $storages;
    protected $pools;
    protected $jobdefaults;

    public function __construct($clients = null, $jobs = null, $filesets = null, $storages = null, $pools = null, $jobdefaults = null)
    {
        parent::__construct('runjob');

        $this->clients = $clients;
        $this->jobs = $jobs;
        $this->filesets = $filesets;
        $this->storages = $storages;
        $this->pools = $pools;
        $this->jobdefaults = $jobdefaults;

        $this->addSelectElement('client', _('Client'), $this->getClientList(), $jobdefaults['client'] ?? null);
        $this->addSelectElement('job', _('Job'), $this->getJobList(), $jobdefaults['job'] ?? null);
        $this->addSelectElement('fileset', _('Fileset'), $this->getFilesetList(), $jobdefaults['fileset'] ?? null);
        $this->addSelectElement('storage', _('Storage'), $this->getStorageList(), $jobdefaults['storage'] ?? null);
        $this->addSelectElement('pool', _('Pool'), $this->getPoolList(), $jobdefaults['pool'] ?? null);
        $this->addSelectElement('level', _('Level'), $this->getLevelList(), $jobdefaults['level'] ?? null);

        // Priority
        $this->add(array(
            'name' => 'priority',
            'type' => 'Laminas\Form\Element\Text',
            'options' => array(
                'label' => _('Priority'),
            ),
            'attributes' => array(
                'class' => 'form-control',
                'id' => 'priority',
                'placeholder' => '10'
            )
        ));

        // Type
        $this->add(array(
            'name' => 'type',
            'type' => 'Laminas\Form\Element\Text',
            'options' => array(
                'label' => _('Type'),
                'empty_option' => '',
            ),
            'attributes' => array(
                'class' => 'form-control',
                'id' => 'type',
                'value' => $jobdefaults['type'] ?? null,
                'readonly' => true
            )
        ));

        $this->addSelectElement(
            'nextpool',
            _('Next Pool'),
            $this->getPoolList(),
            isset($jobdefaults['pool']) ? $this->getNextPool($jobdefaults['pool']) : null
        );

        // When
        $this->add(array(
            'name' => 'when',
            'type' => 'Laminas\Form\Element\Text',
            'options' => array(
                'label' => _('When'),
            ),
            'attributes' => array(
                'class' => 'form-control',
                'id' => 'when',
                'placeholder' => 'YYYY-MM-DD HH:MM:SS'
            )
        ));

        // Submit button
        $this->add(array(
            'name' => 'submit',
            'type' => 'submit',
            'attributes' => array(
                'class' => 'form-control',
                'value' => _('Submit'),
                'id' => 'submit'
            )
        ));

        $this->add(array(
            'name' => 'csrf',
            'type' => 'Laminas\Form\Element\Csrf',
            'options' => array(
                'csrf_options' => array('timeout' => 3600),
            ),
        ));
    }

    private function addSelectElement(string $name, string $label, array $valueOptions, $value): void
    {
        $this->add(array(
            'name' => $name,
            'type' => 'select',
            'options' => array(
                'label' => $label,
                'empty_option' => '',
                'value_options' => $valueOptions
            ),
            'attributes' => array(
                'class' => 'form-control selectpicker show-tick',
                'data-live-search' => 'true',
                'id' => $name,
                'value' => $value
            )
        ));
    }

    private function buildNameList(?array $items): array
    {
        $selectData = [];
        if (!empty($items)) {
            foreach ($items as $item) {
                $selectData[$item['name']] = $item['name'];
            }
            ksort($selectData);
        }
        return $selectData;
    }

    private function getClientList(): array
    {
        return $this->buildNameList($this->clients);
    }

    private function getJobList(): array
    {
        return $this->buildNameList($this->jobs);
    }

    private function getFilesetList(): array
    {
        return $this->buildNameList($this->filesets);
    }

    private function getStorageList(): array
    {
        return $this->buildNameList($this->storages);
    }

    private function getPoolList(): array
    {
        return $this->buildNameList($this->pools);
    }

    private function getLevelList(): array
    {
        $selectData = array();
        $selectData['Full'] = 'Full';
        $selectData['Differential'] = 'Differential';
        $selectData['Incremental'] = 'Incremental';
        $selectData['VirtualFull'] = 'VirtualFull';
        return $selectData;
    }

    private function getNextPool($pool_name)
    {
        foreach ($this->pools as $pool) {
            if ($pool["name"] === $pool_name) {
                return $pool["nextpool"];
            }
        }
    }

    public function getPoolNextPoolMapping(): array
    {
        $mapping = [];

        foreach ($this->pools as $pool) {
            $mapping[$pool["name"]] =  $pool["nextpool"];
        }

        return $mapping;
    }
}
