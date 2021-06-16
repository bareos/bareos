<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2015 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\View\Resolver;

use ArrayObject;
use PHPUnit\Framework\TestCase;
use Zend\View\Resolver\TemplateMapResolver;

class TemplateMapResolverTest extends TestCase
{
    public function testMapIsEmptyByDefault()
    {
        $resolver = new TemplateMapResolver();
        $this->assertEquals([], $resolver->getMap());
    }

    public function testCanSeedMapWithArrayViaConstructor()
    {
        $map = ['foo/bar' => __DIR__ . '/foo/bar.phtml'];
        $resolver = new TemplateMapResolver($map);
        $this->assertEquals($map, $resolver->getMap());
    }

    public function testCanSeedMapWithTraversableViaConstructor()
    {
        $map = new ArrayObject(['foo/bar' => __DIR__ . '/foo/bar.phtml']);
        $resolver = new TemplateMapResolver($map);
        $this->assertEquals($map->getArrayCopy(), $resolver->getMap());
    }

    public function testCanSeedMapWithArrayViaSetter()
    {
        $map = ['foo/bar' => __DIR__ . '/foo/bar.phtml'];
        $resolver = new TemplateMapResolver();
        $resolver->setMap($map);
        $this->assertEquals($map, $resolver->getMap());
    }

    public function testCanSeedMapWithTraversableViaSetter()
    {
        $map = new ArrayObject(['foo/bar' => __DIR__ . '/foo/bar.phtml']);
        $resolver = new TemplateMapResolver();
        $resolver->setMap($map);
        $this->assertEquals($map->getArrayCopy(), $resolver->getMap());
    }

    public function testCanAppendSingleEntriesViaAdd()
    {
        $map = ['foo/bar' => __DIR__ . '/foo/bar.phtml'];
        $resolver = new TemplateMapResolver($map);
        $resolver->add('foo/baz', __DIR__ . '/../foo/baz.phtml');
        $expected = array_merge($map, ['foo/baz' => __DIR__ . '/../foo/baz.phtml']);
        $this->assertEquals($expected, $resolver->getMap());
    }

    public function testCanAppendMultipleEntriesAsArrayViaAdd()
    {
        $map = ['foo/bar' => __DIR__ . '/foo/bar.phtml'];
        $resolver = new TemplateMapResolver($map);
        $more = [
            'foo/baz' => __DIR__ . '/../foo/baz.phtml',
            'baz/bat' => __DIR__ . '/baz/bat.phtml',
        ];
        $resolver->add($more);
        $expected = array_merge($map, $more);
        $this->assertEquals($expected, $resolver->getMap());
    }

    public function testCanAppendMultipleEntriesAsTraversableViaAdd()
    {
        $map = ['foo/bar' => __DIR__ . '/foo/bar.phtml'];
        $resolver = new TemplateMapResolver($map);
        $more = new ArrayObject([
            'foo/baz' => __DIR__ . '/../foo/baz.phtml',
            'baz/bat' => __DIR__ . '/baz/bat.phtml',
        ]);
        $resolver->add($more);
        $expected = array_merge($map, $more->getArrayCopy());
        $this->assertEquals($expected, $resolver->getMap());
    }

    public function testCanAppendMultipleEntriesAsArrayViaMerge()
    {
        $map = ['foo/bar' => __DIR__ . '/foo/bar.phtml'];
        $resolver = new TemplateMapResolver($map);
        $more = [
            'foo/baz' => __DIR__ . '/../foo/baz.phtml',
            'baz/bat' => __DIR__ . '/baz/bat.phtml',
        ];
        $resolver->merge($more);
        $expected = array_merge($map, $more);
        $this->assertEquals($expected, $resolver->getMap());
    }

    public function testCanAppendMultipleEntriesAsTraversableViaMerge()
    {
        $map = ['foo/bar' => __DIR__ . '/foo/bar.phtml'];
        $resolver = new TemplateMapResolver($map);
        $more = new ArrayObject([
            'foo/baz' => __DIR__ . '/../foo/baz.phtml',
            'baz/bat' => __DIR__ . '/baz/bat.phtml',
        ]);
        $resolver->merge($more);
        $expected = array_merge($map, $more->getArrayCopy());
        $this->assertEquals($expected, $resolver->getMap());
    }

    public function testCanMergeTwoMaps()
    {
        $map = ['foo/bar' => __DIR__ . '/foo/bar.phtml'];
        $resolver = new TemplateMapResolver($map);
        $more = new TemplateMapResolver([
            'foo/baz' => __DIR__ . '/../foo/baz.phtml',
            'baz/bat' => __DIR__ . '/baz/bat.phtml',
        ]);
        $resolver->merge($more);
        $expected = array_merge($map, $more->getMap());
        $this->assertEquals($expected, $resolver->getMap());
    }

    public function testAddOverwritesMatchingEntries()
    {
        $map = ['foo/bar' => __DIR__ . '/foo/bar.phtml'];
        $resolver = new TemplateMapResolver($map);
        $more = [
            'foo/bar' => __DIR__ . '/../foo/baz.phtml',
            'baz/bat' => __DIR__ . '/baz/bat.phtml',
        ];
        $resolver->merge($more);
        $expected = array_merge($map, $more);
        $this->assertEquals($expected, $resolver->getMap());
        $this->assertEquals(__DIR__ . '/../foo/baz.phtml', $resolver->get('foo/bar'));
    }

    public function testMergeOverwritesMatchingEntries()
    {
        $map = ['foo/bar' => __DIR__ . '/foo/bar.phtml'];
        $resolver = new TemplateMapResolver($map);
        $more = new TemplateMapResolver([
            'foo/bar' => __DIR__ . '/../foo/baz.phtml',
            'baz/bat' => __DIR__ . '/baz/bat.phtml',
        ]);
        $resolver->merge($more);
        $expected = array_merge($map, $more->getMap());
        $this->assertEquals($expected, $resolver->getMap());
        $this->assertEquals(__DIR__ . '/../foo/baz.phtml', $resolver->get('foo/bar'));
    }

    public function testHasReturnsTrueWhenMatchingNameFound()
    {
        $map = ['foo/bar' => __DIR__ . '/foo/bar.phtml'];
        $resolver = new TemplateMapResolver($map);
        $this->assertTrue($resolver->has('foo/bar'));
    }

    public function testHasReturnsFalseWhenNameHasNoMatch()
    {
        $map = ['foo/bar' => __DIR__ . '/foo/bar.phtml'];
        $resolver = new TemplateMapResolver($map);
        $this->assertFalse($resolver->has('bar/baz'));
    }

    public function testGetReturnsPathWhenNameHasMatch()
    {
        $map = ['foo/bar' => __DIR__ . '/foo/bar.phtml'];
        $resolver = new TemplateMapResolver($map);
        $this->assertEquals($map['foo/bar'], $resolver->get('foo/bar'));
    }

    public function testGetReturnsFalseWhenNameHasNoMatch()
    {
        $map = ['foo/bar' => __DIR__ . '/foo/bar.phtml'];
        $resolver = new TemplateMapResolver($map);
        $this->assertFalse($resolver->get('bar/baz'));
    }

    public function testResolveReturnsPathWhenNameHasMatch()
    {
        $map = ['foo/bar' => __DIR__ . '/foo/bar.phtml'];
        $resolver = new TemplateMapResolver($map);
        $this->assertEquals($map['foo/bar'], $resolver->resolve('foo/bar'));
    }

    public function testResolveReturnsFalseWhenNameHasNoMatch()
    {
        $map = ['foo/bar' => __DIR__ . '/foo/bar.phtml'];
        $resolver = new TemplateMapResolver($map);
        $this->assertFalse($resolver->resolve('bar/baz'));
    }
}
