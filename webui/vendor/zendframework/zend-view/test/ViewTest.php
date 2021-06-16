<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2015 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\View;

use ArrayObject;
use PHPUnit\Framework\TestCase;
use stdClass;
use Zend\Http\Request;
use Zend\Http\Response;
use Zend\View\Exception;
use Zend\View\Model\ViewModel;
use Zend\View\Model\JsonModel;
use Zend\View\Renderer\PhpRenderer;
use Zend\View\Renderer;
use Zend\View\Resolver;
use Zend\View\Variables as ViewVariables;
use Zend\View\View;
use Zend\View\ViewEvent;

class ViewTest extends TestCase
{
    public function setUp()
    {
        $this->request  = new Request;
        $this->response = new Response;
        $this->model    = new ViewModel;
        $this->view     = new View;

        $this->view->setRequest($this->request);
        $this->view->setResponse($this->response);
    }

    public function attachTestStrategies()
    {
        $this->view->addRenderingStrategy(function ($e) {
            return new TestAsset\Renderer\VarExportRenderer();
        });
        $this->result = $result = new stdClass;
        $this->view->addResponseStrategy(function ($e) use ($result) {
            $result->content = $e->getResult();
        });
    }

    public function testRendersViewModelWithNoChildren()
    {
        $this->attachTestStrategies();
        $variables = [
            'foo' => 'bar',
            'bar' => 'baz',
        ];
        $this->model->setVariables($variables);
        $this->view->render($this->model);

        foreach ($variables as $key => $value) {
            $expect = sprintf("'%s' => '%s',", $key, $value);
            $this->assertContains($expect, $this->result->content);
        }
    }

    public function testRendersViewModelWithChildren()
    {
        $this->attachTestStrategies();

        $child1 = new ViewModel(['foo' => 'bar']);

        $child2 = new ViewModel(['bar' => 'baz']);

        $this->model->setVariable('parent', 'node');
        $this->model->addChild($child1, 'child1');
        $this->model->addChild($child2, 'child2');

        $this->view->render($this->model);

        $expected = var_export(new ViewVariables([
            'parent' => 'node',
            'child1' => var_export(['foo' => 'bar'], true),
            'child2' => var_export(['bar' => 'baz'], true),
        ]), true);
        $this->assertEquals($expected, $this->result->content);
    }

    public function testRendersTreeOfModels()
    {
        $this->attachTestStrategies();

        $child1 = new ViewModel(['foo' => 'bar']);
        $child1->setCaptureTo('child1');

        $child2 = new ViewModel(['bar' => 'baz']);
        $child2->setCaptureTo('child2');
        $child1->addChild($child2);

        $this->model->setVariable('parent', 'node');
        $this->model->addChild($child1);

        $this->view->render($this->model);

        $expected = var_export(new ViewVariables([
            'parent' => 'node',
            'child1' => var_export([
                'foo'    => 'bar',
                'child2' => var_export(['bar' => 'baz'], true),
            ], true),
        ]), true);
        $this->assertEquals($expected, $this->result->content);
    }

    public function testChildrenMayInvokeDifferentRenderingStrategiesThanParents()
    {
        $this->view->addRenderingStrategy(function ($e) {
            $model = $e->getModel();
            if (! $model instanceof ViewModel) {
                return;
            }
            return new TestAsset\Renderer\VarExportRenderer();
        });
        $this->view->addRenderingStrategy(function ($e) {
            $model = $e->getModel();
            if (! $model instanceof JsonModel) {
                return;
            }
            return new Renderer\JsonRenderer();
        }, 10); // higher priority, so it matches earlier
        $this->result = $result = new stdClass;
        $this->view->addResponseStrategy(function ($e) use ($result) {
            $result->content = $e->getResult();
        });

        $child1 = new ViewModel(['foo' => 'bar']);
        $child1->setCaptureTo('child1');

        $child2 = new JsonModel(['bar' => 'baz']);
        $child2->setCaptureTo('child2');
        $child2->setTerminal(false);

        $this->model->setVariable('parent', 'node');
        $this->model->addChild($child1);
        $this->model->addChild($child2);

        $this->view->render($this->model);

        $expected = var_export(new ViewVariables([
            'parent' => 'node',
            'child1' => var_export(['foo' => 'bar'], true),
            'child2' => json_encode(['bar' => 'baz']),
        ]), true);
        $this->assertEquals($expected, $this->result->content);
    }

    public function testTerminalChildRaisesException()
    {
        $this->attachTestStrategies();

        $child1 = new ViewModel(['foo' => 'bar']);
        $child1->setCaptureTo('child1');
        $child1->setTerminal(true);

        $this->model->setVariable('parent', 'node');
        $this->model->addChild($child1);

        $this->expectException(Exception\DomainException::class);
        $this->view->render($this->model);
    }

    public function testChildrenAreCapturedToParentVariables()
    {
        // I wish there were a "markTestRedundant()" method in PHPUnit
        $this->testRendersViewModelWithChildren();
    }

    public function testOmittingCaptureToValueInChildLeadsToOmissionInParent()
    {
        $this->attachTestStrategies();

        $child1 = new ViewModel(['foo' => 'bar']);
        $child1->setCaptureTo('child1');

        // Deliberately disable the "capture to" declaration
        $child2 = new ViewModel(['bar' => 'baz']);
        $child2->setCaptureTo(null);

        $this->model->setVariable('parent', 'node');
        $this->model->addChild($child1);
        $this->model->addChild($child2);

        $this->view->render($this->model);

        $expected = var_export(new ViewVariables([
            'parent' => 'node',
            'child1' => var_export(['foo' => 'bar'], true),
        ]), true);
        $this->assertEquals($expected, $this->result->content);
    }

    public function testResponseStrategyIsTriggeredForParentModel()
    {
        // I wish there were a "markTestRedundant()" method in PHPUnit
        $this->testRendersViewModelWithChildren();
    }

    public function testResponseStrategyIsNotTriggeredForChildModel()
    {
        $this->view->addRenderingStrategy(function ($e) {
            return new Renderer\JsonRenderer();
        });

        $result = new ArrayObject;
        $this->view->addResponseStrategy(function ($e) use ($result) {
            $result[] = $e->getResult();
        });

        $child1 = new ViewModel(['foo' => 'bar']);
        $child1->setCaptureTo('child1');

        $child2 = new ViewModel(['bar' => 'baz']);
        $child2->setCaptureTo('child2');

        $this->model->setVariable('parent', 'node');
        $this->model->addChild($child1);
        $this->model->addChild($child2);

        $this->view->render($this->model);

        $this->assertEquals(1, count($result));
    }

    public function testUsesTreeRendererInterfaceToDetermineWhetherOrNotToPassOnlyRootViewModelToPhpRenderer()
    {
        $resolver    = new Resolver\TemplateMapResolver([
            'layout'  => __DIR__ . '/_templates/nested-view-model-layout.phtml',
            'content' => __DIR__ . '/_templates/nested-view-model-content.phtml',
        ]);
        $phpRenderer = new PhpRenderer();
        $phpRenderer->setCanRenderTrees(true);
        $phpRenderer->setResolver($resolver);

        $this->view->addRenderingStrategy(function ($e) use ($phpRenderer) {
            return $phpRenderer;
        });

        $result = new stdClass;
        $this->view->addResponseStrategy(function ($e) use ($result) {
            $result->content = $e->getResult();
        });

        $layout = new ViewModel();
        $layout->setTemplate('layout');
        $content = new ViewModel();
        $content->setTemplate('content');
        $content->setCaptureTo('content');
        $layout->addChild($content);

        $this->view->render($layout);

        $this->assertContains('Layout start', $result->content);
        $this->assertContains('Content for layout', $result->content, $result->content);
        $this->assertContains('Layout end', $result->content);
    }

    public function testUsesTreeRendererInterfaceToDetermineWhetherOrNotToPassOnlyRootViewModelToJsonRenderer()
    {
        $jsonRenderer = new Renderer\JsonRenderer();

        $this->view->addRenderingStrategy(function ($e) use ($jsonRenderer) {
            return $jsonRenderer;
        });

        $result = new stdClass;
        $this->view->addResponseStrategy(function ($e) use ($result) {
            $result->content = $e->getResult();
        });

        $layout  = new ViewModel(['status' => 200]);
        $content = new ViewModel(['foo' => 'bar']);
        $content->setCaptureTo('response');
        $layout->addChild($content);

        $this->view->render($layout);

        $expected = json_encode([
            'status' => 200,
            'response' => ['foo' => 'bar'],
        ]);

        $this->assertEquals($expected, $result->content);
    }

    public function testCanTriggerPostRendererEvent()
    {
        $this->attachTestStrategies();
        $test = (object) ['flag' => false];
        $this->view->getEventManager()->attach(ViewEvent::EVENT_RENDERER_POST, function ($e) use ($test) {
            $test->flag = true;
        });
        $variables = [
            'foo' => 'bar',
            'bar' => 'baz',
        ];
        $this->model->setVariables($variables);
        $this->view->render($this->model);
        $this->assertTrue($test->flag);
    }

    /**
     * Test the view model can be swapped out
     *
     * @see https://github.com/zendframework/zf2/pull/4164
     */
    public function testModelFromEventIsUsedByRenderer()
    {
        $renderer = $this->getMockBuilder(PhpRenderer::class)
            ->setMethods(['render'])
            ->getMock();

        $model1 = new ViewModel;
        $model2 = new ViewModel;

        $this->view->addRenderingStrategy(function ($e) use ($renderer) {
            return $renderer;
        });

        $this->view->getEventManager(ViewEvent::EVENT_RENDERER_POST, function ($e) use ($model2) {
            $e->setModel($model2);
        });

        $renderer->expects($this->once())
                 ->method('render')
                 ->with($model2);

        $this->view->render($model1);
    }
}
