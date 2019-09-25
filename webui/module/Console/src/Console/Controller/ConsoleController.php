<?php

namespace Console\Controller;

use Zend\Mvc\Controller\AbstractActionController;
use Zend\View\Model\ViewModel;
use Zend\Json\Json;

class ConsoleController extends AbstractActionController
{
  /**
   * Variables
   */
  protected $consoleModel = null;
  protected $directorModel = null;
  protected $bsock = null;
  protected $acl_alert = false;

  /**
   * Console Action
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
    $invalid_commands = $this->CommandACLPlugin()->getInvalidCommands(
      $module_config['console_commands']['Console']['mandatory']
    );
    if(count($invalid_commands) > 0) {
      $this->acl_alert = true;
      return new ViewModel(
        array(
          'acl_alert' => $this->acl_alert,
          'invalid_commands' => implode(",", $invalid_commands)
        )
      );
    }

    return new ViewModel();
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
    $command = $this->params()->fromPost('command');

    if($data == "cli") {
      try {
        $this->bsock = $this->getServiceLocator()->get('director');
        $result = $this->getDirectorModel()->sendDirectorCommand($this->bsock, $command);
        $this->bsock->disconnect();
      } catch(Exception $e) {
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
   * Get Director Model
   *
   * @return object
   */
  public function getDirectorModel()
  {
    if(!$this->directorModel) {
      $sm = $this->getServiceLocator();
      $this->directorModel = $sm->get('Director\Model\DirectorModel');
    }
    return $this->directorModel;
  }
}

