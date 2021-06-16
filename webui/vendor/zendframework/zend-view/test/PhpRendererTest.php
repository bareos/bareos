<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2015 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\View;

use PHPUnit\Framework\TestCase;
use Zend\ServiceManager\ServiceManager;
use Zend\View\Exception;
use Zend\View\Helper\Doctype;
use Zend\View\HelperPluginManager;
use Zend\View\Renderer\PhpRenderer;
use Zend\View\Model\ViewModel;
use Zend\View\Resolver\TemplateMapResolver;
use Zend\View\Resolver\TemplatePathStack;
use Zend\View\Variables;
use Zend\Filter\FilterChain;
use ZendTest\View\TestAsset;

/**
 * @group      Zend_View
 */
class PhpRendererTest extends TestCase
{
    public function setUp()
    {
        $this->renderer = new PhpRenderer();
    }

    public function testEngineIsIdenticalToRenderer()
    {
        $this->assertSame($this->renderer, $this->renderer->getEngine());
    }

    public function testUsesTemplatePathStackAsDefaultResolver()
    {
        $this->assertInstanceOf(TemplatePathStack::class, $this->renderer->resolver());
    }

    public function testCanSetResolverInstance()
    {
        $resolver = new TemplatePathStack();
        $this->renderer->setResolver($resolver);
        $this->assertSame($resolver, $this->renderer->resolver());
    }

    public function testPassingNameToResolverReturnsScriptName()
    {
        $this->renderer->resolver()->addPath(__DIR__ . '/_templates');
        $filename = $this->renderer->resolver('test.phtml');
        $this->assertEquals(realpath(__DIR__ . '/_templates/test.phtml'), $filename);
    }

    public function testUsesVariablesObjectForVarsByDefault()
    {
        $this->assertInstanceOf(Variables::class, $this->renderer->vars());
    }

    public function testCanSpecifyArrayAccessForVars()
    {
        $a = new \ArrayObject;
        $this->renderer->setVars($a);
        $this->assertSame($a->getArrayCopy(), $this->renderer->vars()->getArrayCopy());
    }

    public function testCanSpecifyArrayForVars()
    {
        $vars = ['foo' => 'bar'];
        $this->renderer->setVars($vars);
        $this->assertEquals($vars, $this->renderer->vars()->getArrayCopy());
    }

    public function testPassingArgumentToVarsReturnsValueFromThatKey()
    {
        $this->renderer->vars()->assign(['foo' => 'bar']);
        $this->assertEquals('bar', $this->renderer->vars('foo'));
    }

    public function testUsesHelperPluginManagerByDefault()
    {
        $this->assertInstanceOf(HelperPluginManager::class, $this->renderer->getHelperPluginManager());
    }

    public function testPassingArgumentToPluginReturnsHelperByThatName()
    {
        $helper = $this->renderer->plugin('doctype');
        $this->assertInstanceOf(Doctype::class, $helper);
    }

    public function testPassingStringOfUndefinedClassToSetHelperPluginManagerRaisesException()
    {
        $this->expectException(Exception\ExceptionInterface::class);
        $this->expectExceptionMessage('Invalid');
        $this->renderer->setHelperPluginManager('__foo__');
    }

    public function testPassingValidStringClassToSetHelperPluginManagerCreatesIt()
    {
        $this->renderer->setHelperPluginManager(HelperPluginManager::class);
        $this->assertInstanceOf(HelperPluginManager::class, $this->renderer->getHelperPluginManager());
    }

    public function invalidPluginManagers()
    {
        return [
            [true],
            [1],
            [1.0],
            [['foo']],
            [new \stdClass],
        ];
    }

    /**
     * @dataProvider invalidPluginManagers
     */
    public function testPassingInvalidArgumentToSetHelperPluginManagerRaisesException($plugins)
    {
        $this->expectException(Exception\ExceptionInterface::class);
        $this->expectExceptionMessage('must extend');
        $this->renderer->setHelperPluginManager($plugins);
    }

    public function testInjectsSelfIntoHelperPluginManager()
    {
        $plugins = $this->renderer->getHelperPluginManager();
        $this->assertSame($this->renderer, $plugins->getRenderer());
    }

    public function testUsesFilterChainByDefault()
    {
        $this->assertInstanceOf(FilterChain::class, $this->renderer->getFilterChain());
    }

    public function testMaySetExplicitFilterChainInstance()
    {
        $filterChain = new FilterChain();
        $this->renderer->setFilterChain($filterChain);
        $this->assertSame($filterChain, $this->renderer->getFilterChain());
    }

    public function testRenderingAllowsVariableSubstitutions()
    {
        $expected = 'foo INJECT baz';
        $this->renderer->vars()->assign(['bar' => 'INJECT']);
        $this->renderer->resolver()->addPath(__DIR__ . '/_templates');
        $test = $this->renderer->render('test.phtml');
        $this->assertContains($expected, $test);
    }

    public function testRenderingFiltersContentWithFilterChain()
    {
        $expected = 'foo bar baz';
        $this->renderer->getFilterChain()->attach(function ($content) {
            return str_replace('INJECT', 'bar', $content);
        });
        $this->renderer->vars()->assign(['bar' => 'INJECT']);
        $this->renderer->resolver()->addPath(__DIR__ . '/_templates');
        $test = $this->renderer->render('test.phtml');
        $this->assertContains($expected, $test);
    }

    public function testCanAccessHelpersInTemplates()
    {
        $this->renderer->resolver()->addPath(__DIR__ . '/_templates');
        $content = $this->renderer->render('test-with-helpers.phtml');
        foreach (['foo', 'bar', 'baz'] as $value) {
            $this->assertContains("<li>$value</li>", $content);
        }
    }

    /**
     * @group ZF2-68
     */
    public function testCanSpecifyArrayForVarsAndGetAlwaysArrayObject()
    {
        $vars = ['foo' => 'bar'];
        $this->renderer->setVars($vars);
        $this->assertInstanceOf(Variables::class, $this->renderer->vars());
    }

    /**
     * @group ZF2-68
     */
    public function testPassingVariablesObjectToSetVarsShouldUseItDirectory()
    {
        $vars = new Variables(['foo' => '<p>Bar</p>']);
        $this->renderer->setVars($vars);
        $this->assertSame($vars, $this->renderer->vars());
    }

    /**
     * @group ZF2-86
     */
    public function testNestedRenderingRestoresVariablesCorrectly()
    {
        $expected = "inner\n<p>content</p>";
        $this->renderer->resolver()->addPath(__DIR__ . '/_templates');
        $test = $this->renderer->render('testNestedOuter.phtml', ['content' => '<p>content</p>']);
        $this->assertEquals($expected, $test);
    }

    /**
     * @group convenience-api
     */
    public function testPropertyOverloadingShouldProxyToVariablesContainer()
    {
        $this->renderer->foo = '<p>Bar</p>';
        $this->assertEquals($this->renderer->vars('foo'), $this->renderer->foo);
    }

    /**
     * @group convenience-api
     */
    public function testMethodOverloadingShouldReturnHelperInstanceIfNotInvokable()
    {
        $helpers = new HelperPluginManager(new ServiceManager(), ['invokables' => [
            'uninvokable' => TestAsset\Uninvokable::class,
        ]]);
        $this->renderer->setHelperPluginManager($helpers);
        $helper = $this->renderer->uninvokable();
        $this->assertInstanceOf(TestAsset\Uninvokable::class, $helper);
    }

    /**
     * @group convenience-api
     */
    public function testMethodOverloadingShouldInvokeHelperIfInvokable()
    {
        $helpers = new HelperPluginManager(new ServiceManager(), ['invokables' => [
            'invokable' => TestAsset\Invokable::class,
        ]]);
        $this->renderer->setHelperPluginManager($helpers);
        $return = $this->renderer->invokable('it works!');
        $this->assertEquals('ZendTest\View\TestAsset\Invokable::__invoke: it works!', $return);
    }

    /**
     * @group convenience-api
     */
    public function testGetMethodShouldRetrieveVariableFromVariableContainer()
    {
        $this->renderer->foo = '<p>Bar</p>';
        $foo = $this->renderer->get('foo');
        $this->assertSame($this->renderer->vars()->foo, $foo);
    }

    /**
     * @group convenience-api
     */
    public function testRenderingLocalVariables()
    {
        $expected = '10 > 9';
        $this->renderer->vars()->assign(['foo' => '10 > 9']);
        $this->renderer->resolver()->addPath(__DIR__ . '/_templates');
        $test = $this->renderer->render('testLocalVars.phtml');
        $this->assertContains($expected, $test);
    }

    public function testRendersTemplatesInAStack()
    {
        $resolver = new TemplateMapResolver([
            'layout' => __DIR__ . '/_templates/layout.phtml',
            'block'  => __DIR__ . '/_templates/block.phtml',
        ]);
        $this->renderer->setResolver($resolver);

        $content = $this->renderer->render('block');
        $this->assertRegexp('#<body>\s*Block content\s*</body>#', $content);
    }

    /**
     * @group view-model
     */
    public function testCanRenderViewModel()
    {
        $resolver = new TemplateMapResolver([
            'empty' => __DIR__ . '/_templates/empty.phtml',
        ]);
        $this->renderer->setResolver($resolver);

        $model = new ViewModel();
        $model->setTemplate('empty');

        $content = $this->renderer->render($model);
        $this->assertRegexp('/\s*Empty view\s*/s', $content);
    }

    /**
     * @group view-model
     */
    public function testViewModelWithoutTemplateRaisesException()
    {
        $model = new ViewModel();
        $this->expectException(Exception\DomainException::class);
        $content = $this->renderer->render($model);
    }

    /**
     * @group view-model
     */
    public function testRendersViewModelWithVariablesSpecified()
    {
        $resolver = new TemplateMapResolver([
            'test' => __DIR__ . '/_templates/test.phtml',
        ]);
        $this->renderer->setResolver($resolver);

        $model = new ViewModel();
        $model->setTemplate('test');
        $model->setVariable('bar', 'bar');

        $content = $this->renderer->render($model);
        $this->assertRegexp('/\s*foo bar baz\s*/s', $content);
    }

    /**
     * @group view-model
     */
    public function testRenderedViewModelIsRegisteredAsCurrentViewModel()
    {
        $resolver = new TemplateMapResolver([
            'empty' => __DIR__ . '/_templates/empty.phtml',
        ]);
        $this->renderer->setResolver($resolver);

        $model = new ViewModel();
        $model->setTemplate('empty');

        $content = $this->renderer->render($model);
        $helper  = $this->renderer->plugin('view_model');
        $this->assertTrue($helper->hasCurrent());
        $this->assertSame($model, $helper->getCurrent());
    }

    public function testRendererRaisesExceptionInCaseOfExceptionInView()
    {
        $resolver = new TemplateMapResolver([
            'exception' => __DIR__ . '../../Mvc/View/_files/exception.phtml',
        ]);
        $this->renderer->setResolver($resolver);

        $model = new ViewModel();
        $model->setTemplate('exception');

        try {
            $this->renderer->render($model);
            $this->fail('Exception from renderer should propagate');
        } catch (\Exception $e) {
            $this->assertInstanceOf('Exception', $e);
        }
    }

    public function testRendererRaisesExceptionIfResolverCannotResolveTemplate()
    {
        $expected = '10 &gt; 9';
        $this->renderer->vars()->assign(['foo' => '10 > 9']);
        $this->expectException(Exception\RuntimeException::class);
        $this->expectExceptionMessage('could not resolve');
        $test = $this->renderer->render('should-not-find-this');
    }

    public function invalidTemplateFiles()
    {
        return [
            ['/does/not/exists'],
            ['.']
        ];
    }

    /**
     * @dataProvider invalidTemplateFiles
     */
    public function testRendererRaisesExceptionIfResolvedTemplateIsInvalid($template)
    {
        $resolver = new TemplateMapResolver([
            'invalid' => $template,
        ]);

        // @codingStandardsIgnoreStart
        set_error_handler(function ($errno, $errstr) { return true; }, E_WARNING);
        // @codingStandardsIgnoreEnd

        $this->renderer->setResolver($resolver);

        try {
            $this->renderer->render('invalid');
            $caught = false;
        } catch (\Exception $e) {
            $caught = $e;
        }

        restore_error_handler();
        $this->assertInstanceOf(Exception\UnexpectedValueException::class, $caught);
        $this->assertContains('file include failed', $caught->getMessage());
    }

    /**
     * @group view-model
     */
    public function testDoesNotRenderTreesOfViewModelsByDefault()
    {
        $this->assertFalse($this->renderer->canRenderTrees());
    }

    /**
     * @group view-model
     */
    public function testRenderTreesOfViewModelsCapabilityIsMutable()
    {
        $this->renderer->setCanRenderTrees(true);
        $this->assertTrue($this->renderer->canRenderTrees());
        $this->renderer->setCanRenderTrees(false);
        $this->assertFalse($this->renderer->canRenderTrees());
    }

    /**
     * @group view-model
     */
    public function testIfViewModelComposesVariablesInstanceThenRendererUsesIt()
    {
        $model = new ViewModel();
        $model->setTemplate('template');
        $vars  = $model->getVariables();
        $vars['foo'] = 'BAR-BAZ-BAT';

        $resolver = new TemplateMapResolver([
            'template' => __DIR__ . '/_templates/view-model-variables.phtml',
        ]);
        $this->renderer->setResolver($resolver);
        $test = $this->renderer->render($model);
        $this->assertContains('BAR-BAZ-BAT', $test);
    }

    /**
     * @group ZF2-4221
     */
    public function testSharedInstanceHelper()
    {
        $helpers = new HelperPluginManager(new ServiceManager(), [
            'invokables' => [
                'sharedinstance' => TestAsset\SharedInstance::class,
            ],
            'shared' => [
                'sharedinstance' => false,
            ],
        ]);
        $this->renderer->setHelperPluginManager($helpers);

        // new instance always created when shared = false
        $this->assertEquals(1, $this->renderer->sharedinstance());
        $this->assertEquals(1, $this->renderer->sharedinstance());
        $this->assertEquals(1, $this->renderer->sharedinstance());

        $helpers = new HelperPluginManager(new ServiceManager(), [
            'invokables' => [
                'sharedinstance' => TestAsset\SharedInstance::class,
            ],
            'shared' => [
                'sharedinstance' => true,
            ],
        ]);
        $this->renderer->setHelperPluginManager($helpers);
        // use shared instance when shared = true
        $this->assertEquals(1, $this->renderer->sharedinstance());
        $this->assertEquals(2, $this->renderer->sharedinstance());
        $this->assertEquals(3, $this->renderer->sharedinstance());
    }

    public function testDoesNotCallFilterChainIfNoFilterChainWasSet()
    {
        $this->renderer->resolver()->addPath(__DIR__ . '/_templates');

        $result = $this->renderer->render('empty.phtml');

        $this->assertContains('Empty view', $result);
        $this->assertAttributeEmpty('__filterChain', $this->renderer);
    }

    /**
     * @group zend-view-120
     * @see https://github.com/zendframework/zend-view/issues/120
     */
    public function testRendererDoesntUsePreviousRenderedOutputWhenInvokedWithEmptyString()
    {
        $this->renderer->resolver()->addPath(__DIR__ . '/_templates');

        $previousOutput = $this->renderer->render('empty.phtml');

        $actual = $this->renderer->render('');

        $this->assertNotSame($previousOutput, $actual);
    }

    /**
     * @group zend-view-120
     * @see https://github.com/zendframework/zend-view/issues/120
     */
    public function testRendererDoesntUsePreviousRenderedOutputWhenInvokedWithFalse()
    {
        $this->renderer->resolver()->addPath(__DIR__ . '/_templates');

        $previousOutput = $this->renderer->render('empty.phtml');

        $actual = $this->renderer->render(false);

        $this->assertNotSame($previousOutput, $actual);
    }
}
