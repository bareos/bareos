<?php

/**
 * @see       https://github.com/laminas/laminas-mvc for the canonical source repository
 * @copyright https://github.com/laminas/laminas-mvc/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-mvc/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Mvc\Controller\Plugin;

use Laminas\View\Model\ConsoleModel;

class CreateConsoleNotFoundModel extends AbstractPlugin
{
    /**
     * Create a console view model representing a "not found" action
     *
     * @return ConsoleModel
     */
    public function __invoke()
    {
        $viewModel = new ConsoleModel();

        $viewModel->setErrorLevel(1);
        $viewModel->setResult('Page not found');

        return $viewModel;
    }
}
