<?php

/**
 *
 * bareos-webui - Bareos Web-Frontend
 *
 * @link      https://github.com/bareos/bareos for the canonical source repository
 * @copyright Copyright (c) 2013-2019 Bareos GmbH & Co. KG (http://www.bareos.org/)
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

namespace Storage\Controller;

use Zend\Mvc\Controller\AbstractActionController;
use Zend\View\Model\ViewModel;
use Zend\Json\Json;
use Storage\Form\StorageForm;
use Storage\Model\Storage;

class StorageController extends AbstractActionController
{

  /**
   * Variables
   */
  protected $storageModel = null;
  protected $poolModel = null;
  protected $bsock = null;
  protected $acl_alert = false;

  /**
   * Index Action
   *
   * @return object
   */
  public function indexAction()
  {
    $this->RequestURIPlugin()->setRequestURI();

    if(!$this->SessionTimeoutPlugin()->isValid()) {
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
    $unknown_commands = $this->CommandACLPlugin()->getInvalidCommands(
      $module_config['console_commands']['Storage']['mandatory']
    );
    if(count($unknown_commands) > 0) {
      $this->acl_alert = true;
      return new ViewModel(
        array(
          'acl_alert' => $this->acl_alert,
          'unknown_commands' => implode(",", $unknown_commands)
        )
      );
    }

    return new ViewModel(array());
  }

  /**
   * Details Action
   *
   * @return object
   */
  public function detailsAction()
  {
    $this->RequestURIPlugin()->setRequestURI();

    if(!$this->SessionTimeoutPlugin()->isValid()) {
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
    $unknown_commands = $this->CommandACLPlugin()->getInvalidCommands(
      $module_config['console_commands']['Storage']['mandatory']
    );
    if(count($unknown_commands) > 0) {
      $this->acl_alert = true;
      return new ViewModel(
        array(
          'acl_alert' => $this->acl_alert,
          'unknown_commands' => implode(",", $unknown_commands)
        )
      );
    }

    $result = null;

    $action = $this->params()->fromQuery('action');
    $storagename = $this->params()->fromRoute('id');

    try {
      $this->bsock = $this->getServiceLocator()->get('director');
    }
    catch(Exception $e) {
      echo $e->getMessage();
    }

    try {
      $module_config = $this->getServiceLocator()->get('ModuleManager')->getModule('Application')->getConfig();
      $unknown_commands = $this->CommandACLPlugin()->getInvalidCommands(
        $module_config['console_commands']['Storage']['optional']
      );
      if(count($unknown_commands) > 0 && in_array('.pools', $unknown_commands)) {
        $this->acl_alert = true;
        return new ViewModel(
          array(
            'acl_alert' => $this->acl_alert,
            'unknown_commands' => '.pools'
          )
        );
      } else {
        if(isset($_SESSION['bareos']['ac_labelpooltype'])) {
          $pools = $this->getPoolModel()->getDotPools($this->bsock, $_SESSION['bareos']['ac_labelpooltype']);
        }
        else {
          $pools = $this->getPoolModel()->getDotPools($this->bsock, null);
        }
      }
    } catch(Exception $e) {
      echo $e->getMessage();
    }

    try {
      $module_config = $this->getServiceLocator()->get('ModuleManager')->getModule('Application')->getConfig();
      $unknown_commands = $this->CommandACLPlugin()->getInvalidCommands(
        $module_config['console_commands']['Storage']['optional']
      );
      if(count($unknown_commands) > 0 && in_array('status', $unknown_commands)) {
        $this->acl_alert = true;
        return new ViewModel(
          array(
            'acl_alert' => $this->acl_alert,
            'unknown_commands' => 'status'
          )
        );
      } else {
        $slots = $this->getStorageModel()->getSlots($this->bsock, $storagename);
        $drives = array();
        foreach($slots as $slot) {
          if($slot['type'] == 'drive') {
            array_push($drives, $slot['slotnr']);
          }
        }
      }
    } catch(Exception $e) {
      echo $e->getMessage();
    }

    $form = new StorageForm($storagename, $pools, $drives);
    $form->setAttribute('method','post');

    if(empty($action)) {
      return new ViewModel(array(
        'storagename' => $storagename,
        'form' => $form
      ));
    }
    else {
      if($action == "import") {
        $storage = $this->params()->fromQuery('storage');
        $srcslots = $this->params()->fromQuery('srcslots');
        $dstslots = $this->params()->fromQuery('dstslots');

        try {
          $module_config = $this->getServiceLocator()->get('ModuleManager')->getModule('Application')->getConfig();
          $unknown_commands = $this->CommandACLPlugin()->getInvalidCommands(
            $module_config['console_commands']['Storage']['optional']
          );
          if(count($unknown_commands) > 0 && in_array('import', $unknown_commands)) {
            $this->acl_alert = true;
            return new ViewModel(
              array(
                'acl_alert' => $this->acl_alert,
                'unknown_commands' => 'import'
              )
            );
          } else {
            $result = $this->getStorageModel()->importSlots($this->bsock, $storage, $srcslots, $dstslots);
          }
        } catch(Exception $e) {
          echo $e->getMessage();
        }
      }
      elseif($action == "export") {
        $storage = $this->params()->fromQuery('storage');
        $srcslots = $this->params()->fromQuery('srcslots');

        try {
          $module_config = $this->getServiceLocator()->get('ModuleManager')->getModule('Application')->getConfig();
          $unknown_commands = $this->CommandACLPlugin()->getInvalidCommands(
            $module_config['console_commands']['Storage']['optional']
          );
          if(count($unknown_commands) > 0 && in_array('export', $unknown_commands)) {
            $this->acl_alert = true;
            return new ViewModel(
              array(
                'acl_alert' => $this->acl_alert,
                'unknown_commands' => 'export'
              )
            );
          } else {
            $result = $this->getStorageModel()->exportSlots($this->bsock, $storage, $srcslots);
          }
        } catch(Exception $e) {
          echo $e->getMessage();
        }
      }
      elseif($action == "mount") {
        $storage = $this->params()->fromQuery('storage');
        $slot = $this->params()->fromQuery('slot');
        $drive = $this->params()->fromQuery('drive');

        try {
          $module_config = $this->getServiceLocator()->get('ModuleManager')->getModule('Application')->getConfig();
          $unknown_commands = $this->CommandACLPlugin()->getInvalidCommands(
            $module_config['console_commands']['Storage']['optional']
          );
          if(count($unknown_commands) > 0 && in_array('mount', $unknown_commands)) {
            $this->acl_alert = true;
            return new ViewModel(
              array(
                'acl_alert' => $this->acl_alert,
                'unknown_commands' => 'mount'
              )
            );
          } else {
            $result = $this->getStorageModel()->mountSlot($this->bsock, $storage, $slot, $drive);
          }
        } catch(Exception $e) {
          echo $e->getMessage();
        }
      }
      elseif($action == "unmount") {
        $storage = $this->params()->fromQuery('storage');
        $drive = $this->params()->fromQuery('drive');

        try {
          $module_config = $this->getServiceLocator()->get('ModuleManager')->getModule('Application')->getConfig();
          $unknown_commands = $this->CommandACLPlugin()->getInvalidCommands(
            $module_config['console_commands']['Storage']['optional']
          );
          if(count($unknown_commands) > 0 && in_array('unmount', $unknown_commands)) {
            $this->acl_alert = true;
            return new ViewModel(
              array(
                'acl_alert' => $this->acl_alert,
                'unknown_commands' => 'unmount'
              )
            );
          } else {
            $result = $this->getStorageModel()->unmountSlot($this->bsock, $storage, $drive);
          }
        } catch(Exception $e) {
          echo $e->getMessage();
        }
      }
      elseif($action == "release") {
        $storage = $this->params()->fromQuery('storage');
        $drive = $this->params()->fromQuery('srcslots');

        try {
          $module_config = $this->getServiceLocator()->get('ModuleManager')->getModule('Application')->getConfig();
          $unknown_commands = $this->CommandACLPlugin()->getInvalidCommands(
            $module_config['console_commands']['Storage']['optional']
          );
          if(count($unknown_commands) > 0 && in_array('release', $unknown_commands)) {
            $this->acl_alert = true;
            return new ViewModel(
              array(
                'acl_alert' => $this->acl_alert,
                'unknown_commands' => 'release'
              )
            );
          } else {
            $result = $this->getStorageModel()->releaseSlot($this->bsock, $storage, $drive);
          }
        } catch(Exception $e) {
          echo $e->getMessage();
        }
      }
      elseif($action == "label") {
        $request = $this->getRequest();
        if($request->isPost()) {
          $s = new Storage();
          $form->setInputFilter($s->getInputFilter());
          $form->setData( $request->getPost() );
          if($form->isValid()) {
            $storage = $form->getInputFilter()->getValue('storage');
            $pool = $form->getInputFilter()->getValue('pool');
            $drive = $form->getInputFilter()->getValue('drive');
            try {
              $module_config = $this->getServiceLocator()->get('ModuleManager')->getModule('Application')->getConfig();
              $unknown_commands = $this->CommandACLPlugin()->getInvalidCommands(
                $module_config['console_commands']['Storage']['optional']
              );
              if(count($unknown_commands) > 0 && in_array('label', $unknown_commands)) {
                $this->acl_alert = true;
                return new ViewModel(
                  array(
                    'acl_alert' => $this->acl_alert,
                    'unknown_commands' => 'label'
                  )
                );
              } else {
                $result = $this->getStorageModel()->label($this->bsock, $storage, $pool, $drive);
              }
            } catch(Exception $e) {
              echo $e->getMessage();
            }
          }
          else {
            // Form data not valid
          }
        }
      }
      elseif($action == "updateslots") {
        $storage = $this->params()->fromQuery('storage');

        try {
          $module_config = $this->getServiceLocator()->get('ModuleManager')->getModule('Application')->getConfig();
          $unknown_commands = $this->CommandACLPlugin()->getInvalidCommands(
            $module_config['console_commands']['Storage']['optional']
          );
          if(count($unknown_commands) > 0 && in_array('update', $unknown_commands)) {
            $this->acl_alert = true;
            return new ViewModel(
              array(
                'acl_alert' => $this->acl_alert,
                'unknown_commands' => 'update'
              )
            );
          } else {
            $result = $this->getStorageModel()->updateSlots($this->bsock, $storage);
          }
        } catch(Exception $e) {
          echo $e->getMessage();
        }

      }

      try {
        $this->bsock->disconnect();
      }
      catch(Exception $e) {
        echo $e->getMessage();
      }

      return new ViewModel(array(
        'storagename' => $storagename,
        'result' => $result,
        'form' => $form
      ));
    }
  }

  /**
   * Status Action
   *
   * @return object
   */
  public function statusAction()
  {
    $this->RequestURIPlugin()->setRequestURI();

    if(!$this->SessionTimeoutPlugin()->isValid()) {
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
    $unknown_commands = $this->CommandACLPlugin()->getInvalidCommands(
      $module_config['console_commands']['Storage']['optional']
    );
    if(count($unknown_commands) > 0 && in_array('status', $unknown_commands)) {
      $this->acl_alert = true;
      return new ViewModel(
        array(
          'acl_alert' => $this->acl_alert,
          'unknown_commands' => 'status'
        )
      );
    }

    $result = null;

    $storage = $this->params()->fromQuery('storage');

    try {
      $this->bsock = $this->getServiceLocator()->get('director');
      $result = $this->getStorageModel()->statusStorage($this->bsock, $storage);
      $this->bsock->disconnect();
    }
    catch(Exception $e) {
      echo $e->getMessage();
    }

    return new ViewModel(
      array(
        'result' => $result
      )
    );
  }

  /**
   * Get Data Action
   *
   * @return object
   */
  public function getDataAction()
  {
    $this->RequestURIPlugin()->setRequestURI();

    if(!$this->SessionTimeoutPlugin()->isValid()) {
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

    $result = null;

    $data = $this->params()->fromQuery('data');
    $storage = $this->params()->fromQuery('storage');

    if($data == "all") {
      try {
        $this->bsock = $this->getServiceLocator()->get('director');
        $result = $this->getStorageModel()->getStorages($this->bsock);
        $this->bsock->disconnect();
      }
      catch(Exception $e) {
        echo $e->getMessage();
      }
    }
    elseif($data == "statusslots") {
      try {
        $this->bsock = $this->getServiceLocator()->get('director');
        $result = $this->getStorageModel()->getStatusStorageSlots($this->bsock, $storage);
        $this->bsock->disconnect();
      }
      catch(Exception $e) {
        echo $e->getMessage();
      }
    }

    $response = $this->getResponse();
    $response->getHeaders()->addHeaderLine('Content-Type', 'application/json');

    if(isset($result)) {
      $response->setContent(JSON::encode($result));
    }

    return $response;
  }

  /**
   * Get Storage Model
   *
   * @return object
   */
  public function getStorageModel()
  {
    if(!$this->storageModel) {
      $sm = $this->getServiceLocator();
      $this->storageModel = $sm->get('Storage\Model\StorageModel');
    }
    return $this->storageModel;
  }

  /**
   * Get Pool Model
   *
   * @return object
   */
  public function getPoolModel()
  {
    if(!$this->poolModel) {
      $sm = $this->getServiceLocator();
      $this->poolModel = $sm->get('Pool\Model\PoolModel');
    }
    return $this->poolModel;
  }
}
