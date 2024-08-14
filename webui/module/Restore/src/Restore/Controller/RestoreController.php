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

namespace Restore\Controller;

use Zend\Mvc\Controller\AbstractActionController;
use Zend\View\Model\ViewModel;
use Zend\Json\Json;
use Restore\Model\Restore;
use Restore\Form\RestoreForm;
use Exception;

class RestoreController extends AbstractActionController
{
    /**
     * Variables
     */
    protected $restoreModel = null;
    protected $jobModel = null;
    protected $clientModel = null;
    protected $filesetModel = null;

    protected $bsock = null;
    protected $restore_params = null;

    protected $acl_alert = false;
    protected $ndmp_advice_note = false;

    private $directories = null;
    private $files = null;

    /**
     * Index Action
     */
    public function indexAction()
    {
        $this->RequestURIPlugin()->setRequestURI();

        if (!$this->SessionTimeoutPlugin()->isValid()) {
            return $this->redirect()->toRoute(
                'auth',
                array(
                    'action' => 'login'
                ),
                array(
                    'query' => array(
                        'req' => $this->RequestURIPlugin()->getRequestURI(),
                        'dird' => $_SESSION['bareos']['director']
                    )
                )
            );
        }

        $module_config = $this->getServiceLocator()->get('ModuleManager')->getModule('Application')->getConfig();
        $invalid_commands = $this->CommandACLPlugin()->getInvalidCommands(
            $module_config['console_commands']['Restore']['mandatory']
        );
        if (count($invalid_commands) > 0) {
            $this->acl_alert = true;
            return new ViewModel(
                array(
                    'acl_alert' => $this->acl_alert,
                    'invalid_commands' => implode(",", $invalid_commands)
                )
            );
        }

        $this->bsock = $this->getServiceLocator()->get('director');

        $this->setRestoreParams();
        $errors = null;
        $result = null;
        $restore_jobid = null;

        if ($this->restore_params['client'] != null) {
            $this->handleJobId();
            $this->handleJobMerge();
            $backups = $this->getClientBackups();
            $this->updateBvfsCache();
        } else {
            $backups = null;
        }

        try {
            $restore_source_clients = $this->getClientModel()->getClientsWithBackups($this->bsock);
            $restore_target_clients = $this->getClientModel()->getClients($this->bsock);
            $filesets = $this->getFilesetModel()->getDotFilesets($this->bsock);
            $restorejobs = $this->getJobModel()->getRestoreJobs($this->bsock);
            $restorejobresources = $this->getRestoreModel()->getRestoreJobResources($this->bsock, $restorejobs);
        } catch (Exception $e) {
            echo $e->getMessage();
        }

        if (isset($this->restore_params['client']) && empty($backups)) {
            $errors = 'No backups of client <strong>' . $this->restore_params['client'] . '</strong> found.';
        }

        if (isset($this->restore_params['client'])) {
            $this->ndmp_advice_note = $this->isNDMPBackupClient($this->restore_params['client']);
        }

        // Create the form
        $form = new RestoreForm(
            $this->restore_params,
            $restore_source_clients,
            $filesets,
            $restorejobresources,
            $this->restore_params['jobids'],
            $backups,
            $restore_target_clients
        );

        // Set the method attribute for the form
        $form->setAttribute('method', 'post');

        $request = $this->getRequest();

        if ($request->isPost()) {
            $restore = new Restore();
            $form->setInputFilter($restore->getInputFilter());
            $form->setData($request->getPost());

            if ($form->isValid()) {
                $jobid = $form->getInputFilter()->getValue('jobid');
                $client = $form->getInputFilter()->getValue('client');
                $restoreclient = $form->getInputFilter()->getValue('restoreclient');
                $restorejob = $form->getInputFilter()->getValue('restorejob');
                $where = $form->getInputFilter()->getValue('where');
                $fileid = $form->getInputFilter()->getValue('checked_files');
                $dirid = $form->getInputFilter()->getValue('checked_directories');
                $jobids = $form->getInputFilter()->getValue('jobids_hidden');
                $replace = $form->getInputFilter()->getValue('replace');
                $pluginoptions = $form->getInputFilter()->getValue('pluginoptions');

                try {
                    $module_config = $this->getServiceLocator()->get('ModuleManager')->getModule('Application')->getConfig();
                    $invalid_commands = $this->CommandACLPlugin()->getInvalidCommands(
                        $module_config['console_commands']['Restore']['optional']
                    );
                    if (count($invalid_commands) > 0 && in_array('restore', $invalid_commands)) {
                        $this->acl_alert = true;
                        return new ViewModel(
                            array(
                                'acl_alert' => $this->acl_alert,
                                'invalid_commands' => 'restore'
                            )
                        );
                    } else {
                        $result = $this->getRestoreModel()->restore(
                            $this->bsock,
                            $jobid,
                            $client,
                            $restoreclient,
                            $restorejob,
                            $where,
                            $fileid,
                            $dirid,
                            $jobids,
                            $replace,
                            $pluginoptions
                        );
                        $matches = null;
                        preg_match('/JobId=([0-9]*)/i', $result, $matches, PREG_UNMATCHED_AS_NULL);
                        if(isset($matches[1])) {
                            $restore_jobid = (int)$matches[1];
                        }
                        // if $restore_jobid is not set,
                        // could not be started as expected.
                        // We assume the Director message ($result)
                        // contains information about this has happened.
                    }
                } catch (Exception $e) {
                    echo $e->getMessage();
                }

                $this->bsock->disconnect();

                return new ViewModel(array(
                    'restore_params' => $this->restore_params,
                    'form' => $form,
                    'result' => $result,
                    'errors' => $errors,
                    'ndmp_advice_note' => $this->ndmp_advice_note,
                    'restore_jobid' => $restore_jobid
                ));
            } else {
                $this->bsock->disconnect();

                return new ViewModel(array(
                    'restore_params' => $this->restore_params,
                    'form' => $form,
                    'result' => $result,
                    'errors' => $errors,
                    'ndmp_advice_note' => $this->ndmp_advice_note
                ));
            }
        } else {
            $this->bsock->disconnect();

            return new ViewModel(array(
                'restore_params' => $this->restore_params,
                'form' => $form,
                'result' => $result,
                'errors' => $errors,
                'ndmp_advice_note' => $this->ndmp_advice_note
            ));
        }
    }

    public function versionsAction()
    {
        $this->RequestURIPlugin()->setRequestURI();

        if (!$this->SessionTimeoutPlugin()->isValid()) {
            return $this->redirect()->toRoute(
                'auth',
                array(
                    'action' => 'login'
                ),
                array(
                    'query' => array(
                        'req' => $this->RequestURIPlugin()->getRequestURI(),
                        'dird' => $_SESSION['bareos']['director']
                    )
                )
            );
        }

        $module_config = $this->getServiceLocator()->get('ModuleManager')->getModule('Application')->getConfig();
        $invalid_commands = $this->CommandACLPlugin()->getInvalidCommands(
            $module_config['console_commands']['Restore']['mandatory']
        );
        if (count($invalid_commands) > 0) {
            $this->acl_alert = true;
            return new ViewModel(
                array(
                    'acl_alert' => $this->acl_alert,
                    'invalid_commands' => implode(",", $invalid_commands)
                )
            );
        }

        $this->bsock = $this->getServiceLocator()->get('director');

        $this->setRestoreParams();
        $errors = null;
        $result = null;

        if ($this->restore_params['client'] != null) {
            $this->handleJobId();
            $this->handleJobMerge();
            $backups = $this->getClientBackups();
            $this->updateBvfsCache();
        } else {
            $backups = null;
        }

        try {
            $restore_source_clients = $this->getClientModel()->getClientsWithBackups($this->bsock);
            $restore_target_clients = $this->getClientModel()->getClients($this->bsock);
            $filesets = $this->getFilesetModel()->getDotFilesets($this->bsock);
            $restorejobs = $this->getJobModel()->getRestoreJobs($this->bsock);
            $restorejobresources = $this->getRestoreModel()->getRestoreJobResources($this->bsock, $restorejobs);
        } catch (Exception $e) {
            echo $e->getMessage();
        }

        if (isset($this->restore_params['client']) && empty($backups)) {
            $errors = 'No backups of client <strong>' . $this->restore_params['client'] . '</strong> found.';
        }

        if (isset($this->restore_params['client'])) {
            $this->ndmp_advice_note = $this->isNDMPBackupClient($this->restore_params['client']);
        }

        // Create the form
        $form = new RestoreForm(
            $this->restore_params,
            $restore_source_clients,
            $filesets,
            $restorejobresources,
            $this->restore_params['jobids'],
            $backups,
            $restore_target_clients
        );

        // Set the method attribute for the form
        $form->setAttribute('method', 'post');

        $request = $this->getRequest();

        if ($request->isPost()) {
            $restore = new Restore();
            $form->setInputFilter($restore->getInputFilter());
            $form->setData($request->getPost());

            if ($form->isValid()) {
                $jobid = $form->getInputFilter()->getValue('jobid');
                $client = $form->getInputFilter()->getValue('client');
                $restoreclient = $form->getInputFilter()->getValue('restoreclient');
                $restorejob = $form->getInputFilter()->getValue('restorejob');
                $where = $form->getInputFilter()->getValue('where');
                $fileid = $form->getInputFilter()->getValue('checked_files');
                $dirid = $form->getInputFilter()->getValue('checked_directories');
                $jobids = $form->getInputFilter()->getValue('jobids_hidden');
                $replace = $form->getInputFilter()->getValue('replace');
                $pluginoptions = $form->getInputFilter()->getValue('pluginoptions');

                try {
                    $module_config = $this->getServiceLocator()->get('ModuleManager')->getModule('Application')->getConfig();
                    $invalid_commands = $this->CommandACLPlugin()->getInvalidCommands(
                        $module_config['console_commands']['Restore']['optional']
                    );
                    if (count($invalid_commands) > 0 && in_array('restore', $invalid_commands)) {
                        $this->acl_alert = true;
                        return new ViewModel(
                            array(
                                'acl_alert' => $this->acl_alert,
                                'invalid_commands' => 'restore'
                            )
                        );
                    } else {
                        $result = $this->getRestoreModel()->restore(
                            $this->bsock,
                            $jobid,
                            $client,
                            $restoreclient,
                            $restorejob,
                            $where,
                            $fileid,
                            $dirid,
                            $jobids,
                            $replace,
                            $pluginoptions
                        );
                    }
                } catch (Exception $e) {
                    echo $e->getMessage();
                }

                $this->bsock->disconnect();

                return new ViewModel(array(
                    'restore_params' => $this->restore_params,
                    'form' => $form,
                    'result' => $result,
                    'errors' => $errors,
                    'ndmp_advice_note' => $this->ndmp_advice_note,
                    'checked_files' => '',
                    'checked_directories' => ''
                ));
            } else {
                $this->bsock->disconnect();

                return new ViewModel(array(
                    'restore_params' => $this->restore_params,
                    'form' => $form,
                    'result' => $result,
                    'errors' => $errors,
                    'ndmp_advice_note' => $this->ndmp_advice_note
                ));
            }
        } else {
            $this->bsock->disconnect();

            return new ViewModel(array(
                'restore_params' => $this->restore_params,
                'form' => $form,
                'result' => $result,
                'errors' => $errors,
                'ndmp_advice_note' => $this->ndmp_advice_note
            ));
        }
    }

    private function handleJobId()
    {
        if ($this->restore_params['jobid'] == null) {
            try {
                $latestbackup = $this->getClientModel()->getClientBackups($this->bsock, $this->restore_params['client'], "any", "desc", 1);
                if (empty($latestbackup)) {
                    $this->restore_params['jobid'] = null;
                } else {
                    $this->restore_params['jobid'] = $latestbackup[0]['jobid'];
                }
            } catch (Exception $e) {
                echo $e->getMessage();
            }
        }
    }

    private function handleJobMerge()
    {
        try {
            $this->restore_params['jobids'] = $this->getRestoreModel()->getJobIds($this->bsock, $this->restore_params['jobid'], $this->restore_params['mergefilesets']);
        } catch (Exception $e) {
            echo $e->getMessage();
        }
    }

    private function getClientBackups()
    {
        try {
            return $this->getClientModel()->getClientBackups($this->bsock, $this->restore_params['client'], "any", "desc", null);
        } catch (Exception $e) {
            echo $e->getMessage();
        }
    }

    /**
     * Delivers a subtree as Json for JStree
     */
    public function filebrowserAction()
    {
        $this->RequestURIPlugin()->setRequestURI();

        if (!$this->SessionTimeoutPlugin()->isValid()) {
            return $this->redirect()->toRoute(
                'auth',
                array(
                    'action' => 'login'
                ),
                array(
                    'query' => array(
                        'req' => $this->RequestURIPlugin()->getRequestURI(),
                        'dird' => $_SESSION['bareos']['director']
                    )
                )
            );
        }

        $this->setRestoreParams();
        $this->layout('layout/json');

        if ($this->restore_params['client'] != null) {
            $items = $this->buildSubtree();
        } else {
            $items = "{}";
        }

        return new ViewModel(array('items' => $items));
    }

    /**
     * Get the directories
     */
    private function getDirectories()
    {
        $this->directories = null;

        try {
            $this->directories = $this->getRestoreModel()->getDirectories(
                $this->bsock,
                $this->restore_params['jobids'],
                $this->restore_params['id']
            );
        } catch (Exception $e) {
            echo $e->getMessage();
        }
    }

    /**
     * Get the files
     */
    private function getFiles()
    {
        $this->files = null;

        try {
            $this->files = $this->getRestoreModel()->getFiles(
                $this->bsock,
                $this->restore_params['jobids'],
                $this->restore_params['id']
            );
        } catch (Exception $e) {
            echo $e->getMessage();
        }
    }

    /**
     * Get depending JobIds considering the merge criteria
     */
    private function getJobIds()
    {
        try {
            $this->restore_params['jobids'] = $this->getRestoreModel()->getJobIds(
                $this->bsock,
                $this->restore_params['jobid'],
                $this->restore_params['mergefilesets'],
                $this->restore_params['mergejobs']
            );
        } catch (Exception $e) {
            echo $e->getMessage();
        }
    }

    /**
     * Update Bvfs cache
     */
    private function updateBvfsCache()
    {
        try {
            $this->getRestoreModel()->updateBvfsCache(
                $this->bsock,
                $this->restore_params['jobids']
            );
        } catch (Exception $e) {
            echo $e->getMessage();
        }
    }

    private function escapeNameString($n = null)
    {
        $n = preg_replace('/[\x00-\x1F\x7F]/', '', $n);
        $replace_pairs = array('"' => '\"', '\\' => '\\\\');
        $n = strtr($n, $replace_pairs);

        return $n;
    }

    /**
     * Builds a subtree as Json for JStree
     */
    private function buildSubtree()
    {
        $this->bsock = $this->getServiceLocator()->get('director');
        $this->setRestoreParams();
        $this->getJobIds();

        $this->getDirectories();
        $this->getFiles();

        if (!empty($this->directories)) {
            $dnum = count($this->directories);
        } else {
            $dnum = 0;
        }

        if (!empty($this->files)) {
            $fnum = count($this->files);
        } else {
            $fnum = 0;
        }

        $tmp = $dnum;

        // Build Json for JStree
        $items = '[';

        if ($dnum > 0) {
            foreach ($this->directories as $dir) {
                if ($dir['name'] == ".") {
                    --$dnum;
                    next($this->directories);
                } elseif ($dir['name'] == "..") {
                    --$dnum;
                    next($this->directories);
                } else {
                    --$dnum;
                    $items .= '{';
                    $items .= '"id":"-' . $dir['pathid'] . '"';
                    $items .= ',"text":"' . $this->escapeNameString($dir["name"]) . '"';
                    $items .= ',"icon":"glyphicon glyphicon-folder-close"';
                    $items .= ',"state":""';
                    $items .= ',"data":' . \Zend\Json\Json::encode($dir, \Zend\Json\Json::TYPE_OBJECT);
                    $items .= ',"children":true';
                    $items .= '}';
                    if ($dnum > 0) {
                        $items .= ",";
                    }
                }
            }
            if ($fnum > 0) {
                $items .= ",";
            }
        }

        if ($fnum > 0) {
            foreach ($this->files as $file) {
                $items .= '{';
                $items .= '"id":"' . $file["fileid"] . '"';
                $items .= ',"text":"' . $this->escapeNameString($file["name"]) . '"';
                $items .= ',"icon":"glyphicon glyphicon-file"';
                $items .= ',"state":""';
                $items .= ',"data":' . \Zend\Json\Json::encode($file, \Zend\Json\Json::TYPE_OBJECT);
                $items .= '}';
                --$fnum;
                if ($fnum > 0) {
                    $items .= ",";
                }
            }
        }

        $items .= ']';

        $this->bsock->disconnect();

        return $items;
    }

    /**
     * Set the restore parameters
     */
    private function setRestoreParams()
    {
        if ($this->params()->fromQuery('jobid')) {
            $this->restore_params['jobid'] = $this->params()->fromQuery('jobid');
        } else {
            $this->restore_params['jobid'] = null;
        }

        if ($this->params()->fromQuery('client')) {
            $this->restore_params['client'] = $this->params()->fromQuery('client');
        } else {
            $this->restore_params['client'] = null;
        }

        if ($this->params()->fromQuery('restoreclient')) {
            $this->restore_params['restoreclient'] = $this->params()->fromQuery('restoreclient');
        } else {
            $this->restore_params['restoreclient'] = null;
        }

        if ($this->params()->fromQuery('restorejob')) {
            $this->restore_params['restorejob'] = $this->params()->fromQuery('restorejob');
        } else {
            $this->restore_params['restorejob'] = null;
        }

        if ($this->params()->fromQuery('fileset')) {
            $this->restore_params['fileset'] = $this->params()->fromQuery('fileset');
        } else {
            $this->restore_params['fileset'] = null;
        }

        if ($this->params()->fromQuery('before')) {
            $this->restore_params['before'] = $this->params()->fromQuery('before');
        } else {
            $this->restore_params['before'] = null;
        }

        if ($this->params()->fromQuery('where')) {
            $this->restore_params['where'] = $this->params()->fromQuery('where');
        } else {
            $this->restore_params['where'] = null;
        }

        if ($this->params()->fromQuery('pluginoptions')) {
            $this->restore_params['pluginoptions'] = $this->params()->fromQuery('pluginoptions');
        } else {
            $this->restore_params['pluginoptions'] = null;
        }

        if ($this->params()->fromQuery('fileid')) {
            $this->restore_params['fileid'] = $this->params()->fromQuery('fileid');
        } else {
            $this->restore_params['fileid'] = null;
        }

        if ($this->params()->fromQuery('dirid')) {
            $this->restore_params['dirid'] = $this->params()->fromQuery('dirid');
        } else {
            $this->restore_params['dirid'] = null;
        }

        if ($this->params()->fromQuery('id')) {
            $this->restore_params['id'] = $this->params()->fromQuery('id');
        } else {
            $this->restore_params['id'] = null;
        }

        if ($this->params()->fromQuery('limit')) {
            $this->restore_params['limit'] = $this->params()->fromQuery('limit');
        } else {
            $this->restore_params['limit'] = 2000;
        }

        if ($this->params()->fromQuery('jobids')) {
            $this->restore_params['jobids'] = $this->params()->fromQuery('jobids');
        } else {
            $this->restore_params['jobids'] = null;
        }

        if ($this->params()->fromQuery('mergefilesets') !== null) {
            $this->restore_params['mergefilesets'] = $this->params()->fromQuery('mergefilesets');
        } else {
            $this->restore_params['mergefilesets'] = $_SESSION['bareos']['merge_filesets'] ? 1 : 0;
        }

        if ($this->params()->fromQuery('mergejobs') !== null) {
            $this->restore_params['mergejobs'] = $this->params()->fromQuery('mergejobs');
        } else {
            $this->restore_params['mergejobs'] = $_SESSION['bareos']['merge_jobs'] ? 1 : 0;
        }

        if ($this->params()->fromQuery('replace')) {
            $this->restore_params['replace'] = $this->params()->fromQuery('replace');
        } else {
            $this->restore_params['replace'] = null;
        }

        if ($this->params()->fromQuery('replaceoptions')) {
            $this->restore_params['replaceoptions'] = $this->params()->fromQuery('replaceoptions');
        } else {
            $this->restore_params['replaceoptions'] = null;
        }

        if ($this->params()->fromQuery('versions')) {
            $this->restore_params['versions'] = $this->params()->fromQuery('versions');
        } else {
            $this->restore_params['versions'] = null;
        }

    }

    /**
     * Get Data Action
     *
     * @return object
     */
    public function getDataAction()
    {
        $this->RequestURIPlugin()->setRequestURI();

        if (!$this->SessionTimeoutPlugin()->isValid()) {
            return $this->redirect()->toRoute('auth', array('action' => 'login'), array('query' => array('req' => $this->RequestURIPlugin()->getRequestURI(), 'dird' => $_SESSION['bareos']['director'])));
        }

        $result = null;

        $data = $this->params()->fromQuery('data');
        $clientname = $this->params()->fromQuery('clientname');
        $pathid = $this->params()->fromQuery('pathid');
        $filename = $this->params()->fromQuery('filename');

        if ($data == "fileversions") {
            try {
                $this->bsock = $this->getServiceLocator()->get('director');
                $result = $this->getRestoreModel()->getFileVersions($this->bsock, $clientname, $pathid, $filename);
                $this->bsock->disconnect();
            } catch (Exception $e) {
                echo $e->getMessage();
            }
        }

        $response = $this->getResponse();
        $response->getHeaders()->addHeaderLine('Content-Type', 'application/json');

        if (isset($result)) {
            $response->setContent(JSON::encode($result));
        }

        return $response;
    }

    private function isNDMPBackupClient($client = null)
    {
        try {
            $this->bsock = $this->getServiceLocator()->get('director');
            $result = $this->getRestoreModel()->isNDMPBackupClient($this->bsock, $client);
            return $result;
        } catch (Exception $e) {
            echo $e->getMessage();
        }
    }

    /**
     * Get the restore model
     * @return object
     */
    public function getRestoreModel()
    {
        if (!$this->restoreModel) {
            $sm = $this->getServiceLocator();
            $this->restoreModel = $sm->get('Restore\Model\RestoreModel');
        }
        return $this->restoreModel;
    }

    /**
     * Get the job model
     * @return object
     */
    public function getJobModel()
    {
        if (!$this->jobModel) {
            $sm = $this->getServiceLocator();
            $this->jobModel = $sm->get('Job\Model\JobModel');
        }
        return $this->jobModel;
    }

    /**
     * Get the client model
     * @return object
     */
    public function getClientModel()
    {
        if (!$this->clientModel) {
            $sm = $this->getServiceLocator();
            $this->clientModel = $sm->get('Client\Model\ClientModel');
        }
        return $this->clientModel;
    }

    /**
     * Get the fileset model
     * @return object
     */
    public function getFilesetModel()
    {
        if (!$this->filesetModel) {
            $sm = $this->getServiceLocator();
            $this->filesetModel = $sm->get('Fileset\Model\FilesetModel');
        }
        return $this->filesetModel;
    }
}
