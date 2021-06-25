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
use Zend\View\Resolver\PrefixPathStackResolver;
use Zend\View\Resolver\ResolverInterface;

/**
 * Tests for {@see \Zend\View\Resolver\PrefixPathStackResolver}
 *
 * @covers \Zend\View\Resolver\PrefixPathStackResolver
 */
class PrefixPathStackResolverTest extends TestCase
{
    /**
     * @var string
     */
    private $basePath;

    /**
     * {@inheritDoc}
     */
    protected function setUp()
    {
        $this->basePath = realpath(__DIR__ . '/../_templates/prefix-path-stack-resolver');
    }

    public function testResolveWithoutPathPrefixes()
    {
        $resolver = new PrefixPathStackResolver();

        $this->assertNull($resolver->resolve(__DIR__));
        $this->assertNull($resolver->resolve(__FILE__));
        $this->assertNull($resolver->resolve('path/to/foo'));
        $this->assertNull($resolver->resolve('path/to/bar'));
    }

    public function testResolve()
    {
        $resolver = new PrefixPathStackResolver([
            'base1'  => $this->basePath,
            'base2' => $this->basePath . '/baz'
        ]);

        $this->assertEmpty($resolver->resolve('base1/foo'));
        $this->assertSame(realpath($this->basePath . '/bar.phtml'), $resolver->resolve('base1/bar'));
        $this->assertEmpty($resolver->resolve('base2/tab'));
        $this->assertSame(realpath($this->basePath . '/baz/taz.phtml'), $resolver->resolve('base2/taz'));
    }

    public function testResolveWithCongruentPrefix()
    {
        $resolver = new PrefixPathStackResolver([
            'foo'    => $this->basePath,
            'foobar' => $this->basePath . '/baz'
        ]);

        $this->assertSame(realpath($this->basePath . '/bar.phtml'), $resolver->resolve('foo/bar'));
        $this->assertSame(realpath($this->basePath . '/baz/taz.phtml'), $resolver->resolve('foobar/taz'));
    }

    public function testSetCustomPathStackResolver()
    {
        $mockResolver = $this->prophesize(ResolverInterface::class);
        $mockResolver->resolve('/bar', null)->willReturn('1111');
        $mockResolver->resolve('/baz', null)->willReturn('2222');
        $mockResolver->resolve('/tab', null)->willReturn(false);

        $resolver = new PrefixPathStackResolver([
            'foo' => $mockResolver->reveal(),
        ]);

        $this->assertSame('1111', $resolver->resolve('foo/bar'));
        $this->assertSame('2222', $resolver->resolve('foo/baz'));
        $this->assertNull($resolver->resolve('foo/tab'));
    }
}
