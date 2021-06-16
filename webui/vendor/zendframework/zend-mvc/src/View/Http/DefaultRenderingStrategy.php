<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2015 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace Zend\Mvc\View\Http;

use Zend\EventManager\AbstractListenerAggregate;
use Zend\EventManager\EventManagerInterface;
use Zend\Mvc\Application;
use Zend\Mvc\MvcEvent;
use Zend\Stdlib\ResponseInterface as Response;
use Zend\View\Model\ModelInterface as ViewModel;
use Zend\View\View;

class DefaultRenderingStrategy extends AbstractListenerAggregate
{
    /**
     * Layout template - template used in root ViewModel of MVC event.
     *
     * @var string
     */
    protected $layoutTemplate = 'layout';

    /**
     * @var View
     */
    protected $view;

    /**
     * Set view
     *
     * @param  View $view
     * @return DefaultRenderingStrategy
     */
    public function __construct(View $view)
    {
        $this->view = $view;
    }

    /**
     * {@inheritDoc}
     */
    public function attach(EventManagerInterface $events, $priority = 1)
    {
        $this->listeners[] = $events->attach(MvcEvent::EVENT_RENDER, [$this, 'render'], -10000);
        $this->listeners[] = $events->attach(MvcEvent::EVENT_RENDER_ERROR, [$this, 'render'], -10000);
    }

    /**
     * Set layout template value
     *
     * @param  string $layoutTemplate
     * @return DefaultRenderingStrategy
     */
    public function setLayoutTemplate($layoutTemplate)
    {
        $this->layoutTemplate = (string) $layoutTemplate;
        return $this;
    }

    /**
     * Get layout template value
     *
     * @return string
     */
    public function getLayoutTemplate()
    {
        return $this->layoutTemplate;
    }

    /**
     * Render the view
     *
     * @param  MvcEvent $e
     * @return Response|null
     * @throws \Exception
     */
    public function render(MvcEvent $e)
    {
        $result = $e->getResult();
        if ($result instanceof Response) {
            return $result;
        }

        // Martial arguments
        $request   = $e->getRequest();
        $response  = $e->getResponse();
        $viewModel = $e->getViewModel();
        if (!$viewModel instanceof ViewModel) {
            return;
        }

        $view = $this->view;
        $view->setRequest($request);
        $view->setResponse($response);

        $caughtException = null;

        try {
            $view->render($viewModel);
        } catch (\Throwable $ex) {
            $caughtException = $ex;
        } catch (\Exception $ex) {  // @TODO clean up once PHP 7 requirement is enforced
            $caughtException = $ex;
        }

        if ($caughtException !== null) {
            if ($e->getName() === MvcEvent::EVENT_RENDER_ERROR) {
                throw $caughtException;
            }

            $application = $e->getApplication();
            $events      = $application->getEventManager();

            $e->setError(Application::ERROR_EXCEPTION);
            $e->setParam('exception', $caughtException);
            $e->setName(MvcEvent::EVENT_RENDER_ERROR);
            $events->triggerEvent($e);
        }

        return $response;
    }
}
