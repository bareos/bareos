<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2015 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\View\Resolver;

use PHPUnit\Framework\TestCase;
use Prophecy\Argument;
use stdClass;
use Zend\View\Helper\ViewModel as ViewModelHelper;
use Zend\View\Model\ViewModel;
use Zend\View\Renderer\PhpRenderer;
use Zend\View\Resolver\RelativeFallbackResolver;
use Zend\View\Resolver\ResolverInterface;
use Zend\View\Resolver\TemplateMapResolver;
use Zend\View\Resolver\TemplatePathStack;
use Zend\View\Resolver\AggregateResolver;

/**
 * @covers \Zend\View\Resolver\RelativeFallbackResolver
 */
class RelativeFallbackResolverTest extends TestCase
{
    public function testReturnsResourceFromTheSameNameSpaceWithMapResolver()
    {
        $tplMapResolver = new TemplateMapResolver([
            'foo/bar' => 'foo/baz',
        ]);
        $resolver = new RelativeFallbackResolver($tplMapResolver);
        $renderer = new PhpRenderer();
        $view = new ViewModel();
        $view->setTemplate('foo/zaz');
        $helper = $renderer->plugin('view_model');
        /* @var $helper ViewModelHelper */
        $helper->setCurrent($view);

        $test = $resolver->resolve('bar', $renderer);
        $this->assertEquals('foo/baz', $test);
    }

    public function testReturnsResourceFromTheSameNameSpaceWithPathStack()
    {
        $pathStack = new TemplatePathStack();
        $pathStack->addPath(__DIR__ . '/../_templates');
        $resolver = new RelativeFallbackResolver($pathStack);
        $renderer = new PhpRenderer();
        $view = new ViewModel();
        $view->setTemplate('name-space/any-view');
        /* @var $helper ViewModelHelper */
        $helper = $renderer->plugin('view_model');
        $helper->setCurrent($view);

        $test = $resolver->resolve('bar', $renderer);
        $this->assertEquals(realpath(__DIR__ . '/../_templates/name-space/bar.phtml'), $test);
    }

    public function testReturnsResourceFromTopLevelIfExistsInsteadOfTheSameNameSpace()
    {
        $tplMapResolver = new TemplateMapResolver([
            'foo/bar' => 'foo/baz',
            'bar' => 'baz',
        ]);
        $resolver = new AggregateResolver();
        $resolver->attach($tplMapResolver);
        $resolver->attach(new RelativeFallbackResolver($tplMapResolver));
        $renderer = new PhpRenderer();
        $view = new ViewModel();
        $view->setTemplate('foo/zaz');
        $helper = $renderer->plugin('view_model');
        /* @var $helper ViewModelHelper */
        $helper->setCurrent($view);

        $test = $resolver->resolve('bar', $renderer);
        $this->assertEquals('baz', $test);
    }

    public function testSkipsResolutionOnViewRendererWithoutPlugins()
    {
        $baseResolver = $this->prophesize(ResolverInterface::class);
        $baseResolver->resolve()->shouldNotBeCalled();
        $fallback = new RelativeFallbackResolver($baseResolver->reveal());

        $renderer = $this->prophesize(PhpRenderer::class)->reveal();

        $this->assertFalse($fallback->resolve('foo/bar', $renderer));
    }

    public function testSkipsResolutionOnViewRendererWithoutCorrectCurrentPlugin()
    {
        $baseResolver = $this->prophesize(ResolverInterface::class);
        $baseResolver->resolve()->shouldNotBeCalled();

        $fallback = new RelativeFallbackResolver($baseResolver->reveal());

        $renderer = $this->prophesize(PhpRenderer::class);
        $renderer->plugin(Argument::any())->willReturn(new stdClass())->shouldBeCalledTimes(1);

        $this->assertFalse($fallback->resolve('foo/bar', $renderer->reveal()));
    }

    public function testSkipsResolutionOnNonExistingCurrentViewModel()
    {
        $baseResolver = $this->prophesize(ResolverInterface::class);
        $baseResolver->resolve()->shouldNotBeCalled();

        $fallback  = new RelativeFallbackResolver($baseResolver->reveal());
        $viewModel = new ViewModelHelper();

        $renderer = $this->prophesize(PhpRenderer::class);
        $renderer->plugin(Argument::any())->willReturn($viewModel)->shouldBeCalledTimes(1);

        $this->assertFalse($fallback->resolve('foo/bar', $renderer->reveal()));
    }
}
