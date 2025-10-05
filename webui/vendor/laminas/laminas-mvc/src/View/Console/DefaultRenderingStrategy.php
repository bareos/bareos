<?php

/**
 * @see       https://github.com/laminas/laminas-mvc for the canonical source repository
 * @copyright https://github.com/laminas/laminas-mvc/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-mvc/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Mvc\View\Console;

use Laminas\Console\Response as ConsoleResponse;
use Laminas\EventManager\AbstractListenerAggregate;
use Laminas\EventManager\EventManagerInterface;
use Laminas\Mvc\MvcEvent;
use Laminas\Stdlib\ResponseInterface as Response;
use Laminas\View\Model\ConsoleModel as ConsoleViewModel;
use Laminas\View\Model\ModelInterface;

class DefaultRenderingStrategy extends AbstractListenerAggregate
{
    /**
     * {@inheritDoc}
     */
    public function attach(EventManagerInterface $events)
    {
        $this->listeners[] = $events->attach(MvcEvent::EVENT_RENDER, [$this, 'render'], -10000);
    }

    /**
     * Render the view
     *
     * @param  MvcEvent $e
     * @return Response
     */
    public function render(MvcEvent $e)
    {
        $result = $e->getResult();
        if ($result instanceof Response) {
            return $result; // the result is already rendered ...
        }

        // marshal arguments
        $response  = $e->getResponse();

        if (!$result instanceof ModelInterface) {
            // There is absolutely no result, so there's nothing to display.
            // We will return an empty response object
            return $response;
        }

        // Collect results from child models
        $responseText = '';
        if ($result->hasChildren()) {
            /* @var ModelInterface $child */
            foreach ($result->getChildren() as $child) {
                // Do not use ::getResult() method here as we cannot be sure if
                // children are also console models.
                $responseText .= $child->getVariable(ConsoleViewModel::RESULT);
            }
        }

        // Fetch result from primary model
        if ($result instanceof ConsoleViewModel) {
            $responseText .= $result->getResult();
        } else {
            $responseText .= $result->getVariable(ConsoleViewModel::RESULT);
        }

        // Fetch service manager
        $sm = $e->getApplication()->getServiceManager();

        // Fetch console
        $console = $sm->get('console');

        // Append console response to response object
        $content = $response->getContent() . $responseText;
        if (is_callable([$console, 'encodeText'])) {
            $content = $console->encodeText($content);
        }
        $response->setContent($content);

        // Pass on console-specific options
        if ($response instanceof ConsoleResponse
            && $result instanceof ConsoleViewModel
        ) {
            $errorLevel = $result->getErrorLevel();
            $response->setErrorLevel($errorLevel);
        }

        return $response;
    }
}
