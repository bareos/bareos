<?php

/**
 *
 * bareos-webui - Bareos Web-Frontend
 *
 * @link      https://github.com/bareos/bareos for the canonical source repository
 * @copyright Copyright (C) 2013-2024 Bareos GmbH & Co. KG (http://www.bareos.org/)
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

namespace Restore\Form;

use Zend\Form\Form;
use Zend\Form\Element;

class RestoreForm extends Form
{
    protected $restore_params;
    protected $clients;
    protected $filesets;
    protected $restorejobresources;
    protected $jobids;
    public $backups;

    public function __construct($restore_params = null, $restore_source_clients = null, $filesets = null, $restorejobresources = null, $jobids = null, $backups = null, $restore_target_clients = null, $restore_jobid = null)
    {
        parent::__construct('restore');

        $this->restore_params = $restore_params;
        $this->restore_source_clients = $this->getNameOptionsMap($restore_source_clients);
        $this->filesets = $filesets;
        $this->restorejobresources = $restorejobresources;
        $this->jobids = $jobids;
        $this->backups = $backups;
        $this->restore_target_clients = $this->getNameOptionsMap($restore_target_clients);
        $this->restore_jobid = $restore_jobid;

        // Backup jobs
        if (isset($restore_params['jobid'])) {
            $this->add(array(
                'name' => 'backups',
                'type' => 'select',
                'options' => array(
                    'label' => _('Backup jobs'),
                    'empty_option' => _('Please choose a backup'),
                    'value_options' => $this->getBackupList()
                ),
                'attributes' => array(
                    'class' => 'form-control selectpicker show-tick',
                    'data-live-search' => 'true',
                    'id' => 'jobid',
                    'value' => $restore_params['jobid']
                )
            ));
        } else {
            $this->add(array(
                'name' => 'backups',
                'type' => 'select',
                'options' => array(
                    'label' => _('Backups'),
                    'empty_option' => _('Please choose a backup'),
                    'value_options' => $this->getBackupList()
                ),
                'attributes' => array(
                    'class' => 'form-control selectpicker show-tick',
                    'data-live-search' => 'true',
                    'id' => 'jobid',
                    'disabled' => true
                )
            ));
        }

        // Client
        $this->add(array(
            'name' => 'client',
            'type' => 'select',
            'options' => array(
                'label' => _('Client'),
                'empty_option' => _('Please choose a client'),
                'value_options' => $this->restore_source_clients
            ),
            'attributes' => array(
                'class' => 'form-control selectpicker show-tick',
                'data-live-search' => 'true',
                'id' => 'client',
                'value' => $restore_params['client'] ?? ''
            )
        ));

        // Restore client
        if (isset($this->restore_params['restoreclient']) || isset($this->restore_params['client'])) {
            $this->add(array(
                'name' => 'restoreclient',
                'type' => 'select',
                'options' => array(
                    'label' => _('Restore to client'),
                    'empty_option' => _('Please choose a client'),
                    'value_options' => $this->restore_target_clients
                ),
                'attributes' => array(
                    'class' => 'form-control selectpicker show-tick',
                    'data-live-search' => 'true',
                    'id' => 'restoreclient',
                    'value' => $this->restore_params['restoreclient'] ?? $this->restore_params['client']
                )
            ));
        } else {
            $this->add(array(
                'name' => 'restoreclient',
                'type' => 'select',
                'options' => array(
                    'label' => _('Restore to (another) client'),
                    'empty_option' => _('Please choose a client'),
                    'value_options' => $this->restore_target_clients
                ),
                'attributes' => array(
                    'class' => 'form-control selectpicker show-tick',
                    'data-live-search' => 'true',
                    'id' => 'restoreclient',
                    'disabled' => true
                )
            ));
        }

        // Fileset
        if (isset($restore_params['fileset'])) {
            $this->add(array(
                'name' => 'fileset',
                'type' => 'select',
                'options' => array(
                    'label' => _('Fileset'),
                    'empty_option' => _('Please choose a fileset'),
                    'value_options' => $this->getFilesetList()
                ),
                'attributes' => array(
                    'id' => 'fileset',
                    'value' => $restore_params['fileset']
                )
            ));
        } else {
            $this->add(array(
                'name' => 'fileset',
                'type' => 'select',
                'options' => array(
                    'label' => _('Fileset'),
                    'empty_option' => _('Please choose a fileset'),
                    'value_options' => $this->getFilesetList()
                ),
                'attributes' => array(
                    'id' => 'fileset'
                )
            ));
        }

        // Restore Job
        if (isset($restore_params['restorejob'])) {
            $this->add(array(
                'name' => 'restorejob',
                'type' => 'select',
                'options' => array(
                    'label' => _('Restore job'),
                    //'empty_option' => _('Please choose a restore job'),
                    'value_options' => $this->getRestoreJobList()
                ),
                'attributes' => array(
                    'class' => 'form-control selectpicker show-tick',
                    'data-live-search' => 'true',
                    'id' => 'restorejob',
                    'value' => $restore_params['restorejob']
                )
            ));
        } else {
            if (!empty($this->restore_params['client']) && count($this->getRestoreJobList()) > 0) {
                $this->add(array(
                    'name' => 'restorejob',
                    'type' => 'select',
                    'options' => array(
                        'label' => _('Restore job'),
                        'empty_option' => _('Please choose a restore job'),
                        'value_options' => $this->getRestoreJobList()
                    ),
                    'attributes' => array(
                        'class' => 'form-control selectpicker show-tick',
                        'data-live-search' => 'true',
                        'id' => 'restorejob',
                        'value' => @array_pop($this->getRestoreJobList())
                    )
                ));
            } else {
                $this->add(array(
                    'name' => 'restorejob',
                    'type' => 'select',
                    'options' => array(
                        'label' => _('Restore job'),
                        'empty_option' => _('Please choose a restore job'),
                        'value_options' => $this->getRestoreJobList()
                    ),
                    'attributes' => array(
                        'class' => 'form-control selectpicker show-tick',
                        'data-live-search' => 'true',
                        'id' => 'restorejob',
                        'disabled' => true
                    )
                ));
            }
        }

        // Merge filesets
        if (isset($this->restore_params['client']) && isset($restore_params['mergefilesets'])) {
            $this->add(
                array(
                    'name' => 'mergefilesets',
                    'type' => 'select',
                    'options' => array(
                        'label' => _('Merge all client filesets'),
                        'value_options' => array(
                            '1' => _('Yes'),
                            '0' => _('No')
                        )
                    ),
                    'attributes' => array(
                        'class' => 'form-control selectpicker show-tick',
                        'id' => 'mergefilesets',
                        'value' => $restore_params['mergefilesets']
                    )
                )
            );
        } else {
            $this->add(
                array(
                    'name' => 'mergefilesets',
                    'type' => 'select',
                    'options' => array(
                        'label' => _('Merge all client filesets'),
                        'value_options' => array(
                            '1' => _('Yes'),
                            '0' => _('No')
                        )
                    ),
                    'attributes' => array(
                        'class' => 'form-control selectpicker show-tick',
                        'id' => 'mergefilesets',
                        'value' => $_SESSION['bareos']['merge_filesets'] ? 1 : 0,
                        'disabled' => true
                    )
                )
            );
        }

        // Merge jobs
        if (isset($this->restore_params['client']) && isset($restore_params['mergejobs'])) {
            $this->add(
                array(
                    'name' => 'mergejobs',
                    'type' => 'select',
                    'options' => array(
                        'label' => _('Merge all related jobs to last full backup of selected backup job'),
                        'value_options' => array(
                            '1' => _('Yes'),
                            '0' => _('No')
                        )
                    ),
                    'attributes' => array(
                        'class' => 'form-control selectpicker show-tick',
                        'id' => 'mergejobs',
                        'value' => $restore_params['mergejobs']
                    )
                )
            );
        } else {
            $this->add(
                array(
                    'name' => 'mergejobs',
                    'type' => 'select',
                    'options' => array(
                        'label' => _('Merge jobs'),
                        'value_options' => array(
                            '1' => _('Yes'),
                            '0' => _('No')
                        )
                    ),
                    'attributes' => array(
                        'class' => 'form-control selectpicker show-tick',
                        'id' => 'mergejobs',
                        'value' => $_SESSION['bareos']['merge_jobs'] ? 1 : 0,
                        'disabled' => true
                    )
                )
            );
        }

        // Replace
        if (isset($this->restore_params['restorejob'])) {
            $this->add(
                array(
                    'name' => 'replace',
                    'type' => 'select',
                    'options' => array(
                        'label' => _('Replace files on client'),
                        'value_options' => array(
                            'always' => _('always'),
                            'never' => _('never'),
                            'ifolder' => _('if file being restored is older than existing file'),
                            'ifnewer' => _('if file being restored is newer than existing file')
                        )
                    ),
                    'attributes' => array(
                        'class' => 'form-control selectpicker show-tick',
                        'id' => 'replace',
                        'value' => $this->determineReplaceDirective($this->restore_params['restorejob'])
                    )
                )
            );
        } else {
            if (isset($restore_params['client']) && count($this->getRestoreJobList()) > 0) {
                $this->add(
                    array(
                        'name' => 'replace',
                        'type' => 'select',
                        'options' => array(
                            'label' => _('Replace files on client'),
                            'value_options' => array(
                                'always' => _('always'),
                                'never' => _('never'),
                                'ifolder' => _('if file being restored is older than existing file'),
                                'ifnewer' => _('if file being restored is newer than existing file')
                            )
                        ),
                        'attributes' => array(
                            'class' => 'form-control selectpicker show-tick',
                            'id' => 'replace',
                            'value' => @array_pop($this->getRestoreJobReplaceDirectives()),
                        )
                    )
                );
            } else {
                $this->add(
                    array(
                        'name' => 'replace',
                        'type' => 'select',
                        'options' => array(
                            'label' => _('Replace files on client'),
                            'value_options' => array(
                                'always' => _('always'),
                                'never' => _('never'),
                                'ifolder' => _('if file being restored is older than existing file'),
                                'ifnewer' => _('if file being restored is newer than existing file')
                            )
                        ),
                        'attributes' => array(
                            'class' => 'form-control selectpicker show-tick',
                            'id' => 'replace',
                            'value' => 'always',
                            'disabled' => true
                        )
                    )
                );
            }
        }

        // Plugin Options
        $pluginoptions_placeholder = _('e.g. <plugin>:file=<filepath>:reader=<readprogram>:writer=<writeprogram>');
        if ($restore_params['mergefilesets'] == 0) {
            $pluginjobs = false;
            foreach ($backups as $backup) {
                if ($backup['pluginjob']) {
                    $pluginjobs = true;
                }
            }
            if ($pluginjobs) {
                $this->add(
                    array(
                        'name' => 'pluginoptions',
                        'type' => 'text',
                        'options' => array(
                            'label' => _('Plugin Options')
                        ),
                        'attributes' => array(
                            'class' => 'form-control selectpicker show-tick',
                            'value' => '',
                            'id' => 'pluginoptions',
                            'size' => '30',
                            'placeholder' => $pluginoptions_placeholder,
                            'required' => false
                        )
                    )
                );
            } else {
                $this->add(
                    array(
                        'name' => 'pluginoptions',
                        'type' => 'Zend\Form\Element\Hidden',
                        'attributes' => array(
                            'value' => '',
                            'id' => 'pluginoptions',
                            'required' => false,
                            'disabled' => true
                        )
                    )
                );
            }
        }

        if ($restore_params['mergefilesets'] == 1) {
            if (isset($restore_params['jobid'])) {
                foreach ($backups as $backup) {
                    if ($backup['jobid'] === $restore_params['jobid'] && $backup['pluginjob']) {
                        $this->add(
                            array(
                                'name' => 'pluginoptions',
                                'type' => 'text',
                                'options' => array(
                                    'label' => _('Plugin Options')
                                ),
                                'attributes' => array(
                                    'class' => 'form-control selectpicker show-tick',
                                    'value' => '',
                                    'id' => 'pluginoptions',
                                    'size' => '30',
                                    'placeholder' => $pluginoptions_placeholder,
                                    'required' => false
                                )
                            )
                        );
                    }
                }
            } else {
                $this->add(
                    array(
                        'name' => 'pluginoptions',
                        'type' => 'Zend\Form\Element\Hidden',
                        'attributes' => array(
                            'value' => '',
                            'id' => 'pluginoptions',
                            'required' => false,
                            'disabled' => true
                        )
                    )
                );
            }
        }

        // Where
        $where = null;
        $where_path_of = null;
        if (isset($this->restore_params['restorejob'])) {
            $where_path_of = $this->restore_params['restorejob'];
        } elseif (count($this->restorejobresources) == 1) {
            // get first key of an array
            $where_path_of = key($this->restorejobresources);
        }

        if (isset($this->restorejobresources[$where_path_of]['where'])) {
            $where = $this->restorejobresources[$where_path_of]['where'];
        }

        $this->add(
            array(
                'name' => 'where',
                'type' => 'text',
                'options' => array(
                    'label' => _('Restore location on client')
                ),
                'attributes' => array(
                    'class' => 'form-control selectpicker show-tick',
                    'value' => $where,
                    'id' => 'where',
                    'size' => '30',
                    'placeholder' => _('e.g. / or /tmp/bareos-restores/'),
                    'required' => 'required',
                    'disabled' => (!isset($restore_params['client']))
                )
            )
        );

        // JobIds hidden
        $this->add(
            array(
                'name' => 'jobids_hidden',
                'type' => 'Zend\Form\Element\Hidden',
                'attributes' => array(
                    'value' => $this->getJobIds(),
                    'id' => 'jobids_hidden'
                )
            )
        );

        // JobIds display (for debugging)
        $this->add(
            array(
                'name' => 'jobids_display',
                'type' => 'text',
                'options' => array(
                    'label' => _('Related jobs for a most recent full restore')
                ),
                'attributes' => array(
                    'value' => $this->getJobIds(),
                    'id' => 'jobids_display',
                    'readonly' => true
                )
            )
        );

        // filebrowser checked files
        $this->add(
            array(
                'name' => 'checked_files',
                'type' => 'Zend\Form\Element\Hidden',
                'attributes' => array(
                    'value' => '',
                    'id' => 'checked_files'
                )
            )
        );

        // filebrowser checked directories
        $this->add(
            array(
                'name' => 'checked_directories',
                'type' => 'Zend\Form\Element\Hidden',
                'attributes' => array(
                    'value' => '',
                    'id' => 'checked_directories'
                )
            )
        );

        // Submit button
        $form_submit = array(
            'name' => 'form-submit',
            'type' => 'button',
            'options' => array(
                'label' => _('Restore')
            ),
            'attributes' => array(
                //'value' => _('Restore'),
                'id' => 'btn-form-submit',
                'class' => 'btn btn-primary',
                // will be enabled by JS
                'disabled' => true
            )
        );
        $this->add($form_submit);

    }

    /**
     * Convert an Bareos result array so that it is usable as Form Options array.
     * Here it sets key and value to the "name" field of the Bareos array.
     */
    private function getNameOptionsMap($source)
    {
        $selectData = array();
        if (!empty($source)) {
            foreach ($source as $i) {
                $selectData[$i["name"]] = $i["name"];
            }
        }
        ksort($selectData);
        return $selectData;
    }

    /**
     *
     */
    private function getJobList()
    {
        $selectData = array();
        if (!empty($this->jobs)) {
            foreach ($this->jobs as $job) {
                $selectData[$job['jobid']] = $job['jobid'] . " - " . $job['name'];
            }
        }
        return $selectData;
    }

    /**
     *
     */
    private function getBackupList()
    {
        $selectData = array();
        if (!empty($this->backups)) {
            foreach ($this->backups as $backup) {
                switch ($backup['level']) {
                    case 'I':
                        $level = "Incremental";
                        break;
                    case 'D':
                        $level = "Differential";
                        break;
                    case 'F':
                        $level = "Full";
                        break;
                }
                $selectData[$backup['jobid']] = "(" . $backup['jobid']  . ") " . $backup['starttime'] . " - " . $backup['name'] . " - " . $level;
            }
        }
        return $selectData;
    }

    /**
     *
     */
    private function getJobIds()
    {
        return $this->jobids;
    }

    /**
     *
     */
    private function getFilesetList()
    {
        $selectData = array();
        if (!empty($this->filesets)) {
            foreach ($this->filesets as $fileset) {
                $selectData[$fileset['name']] = $fileset['name'];
            }
        }
        return $selectData;
    }

    /**
     *
     */
    private function getRestoreJobList()
    {
        $selectData = array();
        foreach ($this->restorejobresources as $key => $value) {
            $selectData[$key] = $key;
        }
        return $selectData;
    }

    /**
     *
     */
    private function getRestoreJobReplaceDirectives()
    {
        $selectData = array();
        if (!empty($this->restorejobresources)) {
            foreach ($this->restorejobresources as $restorejob) {
                $selectData[$restorejob['replace']] = $restorejob['replace'];
            }
        }
        return $selectData;
    }

    /**
     *
     */
    private function determineReplaceDirective($restorejob = null)
    {
        $replace = null;
        if (isset($restorejob)) {
            foreach ($this->restorejobresources as $restorejobresource) {
                if ($restorejobresource['job'] == $restorejob) {
                    $replace = $restorejobresource['replace'];
                }
            }
        }
        return $replace;
    }
}
