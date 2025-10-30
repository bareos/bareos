<?php

/**
 * @see       https://github.com/laminas/laminas-mvc for the canonical source repository
 * @copyright https://github.com/laminas/laminas-mvc/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-mvc/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Mvc\Controller\Plugin;

use Laminas\Mvc\Exception;
use Laminas\Mvc\InjectApplicationEventInterface;
use Laminas\Mvc\MvcEvent;
use Laminas\View\Model\ModelInterface as Model;

class Layout extends AbstractPlugin
{
    /**
     * @var MvcEvent
     */
    protected $event;

    /**
     * Set the layout template
     *
     * @param  string $template
     * @return Layout
     */
    public function setTemplate($template)
    {
        $viewModel = $this->getViewModel();
        $viewModel->setTemplate((string) $template);
        return $this;
    }

    /**
     * Invoke as a functor
     *
     * If no arguments are given, grabs the "root" or "layout" view model.
     * Otherwise, attempts to set the template for that view model.
     *
     * @param  null|string $template
     * @return Model|Layout
     */
    public function __invoke($template = null)
    {
        if (null === $template) {
            return $this->getViewModel();
        }
        return $this->setTemplate($template);
    }

    /**
     * Get the event
     *
     * @return MvcEvent
     * @throws Exception\DomainException if unable to find event
     */
    protected function getEvent()
    {
        if ($this->event) {
            return $this->event;
        }

        $controller = $this->getController();
        if (!$controller instanceof InjectApplicationEventInterface) {
            throw new Exception\DomainException('Layout plugin requires a controller that implements InjectApplicationEventInterface');
        }

        $event = $controller->getEvent();
        if (!$event instanceof MvcEvent) {
            $params = $event->getParams();
            $event  = new MvcEvent();
            $event->setParams($params);
        }
        $this->event = $event;

        return $this->event;
    }

    /**
     * Retrieve the root view model from the event
     *
     * @return Model
     * @throws Exception\DomainException
     */
    protected function getViewModel()
    {
        $event     = $this->getEvent();
        $viewModel = $event->getViewModel();
        if (!$viewModel instanceof Model) {
            throw new Exception\DomainException('Layout plugin requires that event view model is populated');
        }
        return $viewModel;
    }
}
