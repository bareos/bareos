<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2017 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Navigation\Page;

use PHPUnit\Framework\TestCase;
use Zend\Config;
use Zend\Navigation;
use Zend\Navigation\Exception;
use Zend\Navigation\Page\AbstractPage;
use Zend\Navigation\Page\Mvc;
use Zend\Navigation\Page\Uri;

/**
 * Tests the class Zend_Navigation_Page
 *
 * @author    Robin Skoglund
 * @group      Zend_Navigation
 */
class PageTest extends TestCase
{
    /**
     * Prepares the environment before running a test.
     *
     */
    protected function setUp()
    {
    }

    /**
     * Tear down the environment after running a test
     *
     */
    protected function tearDown()
    {
        // setConfig, setOptions
    }

    public function testSetShouldMapToNativeProperties()
    {
        $page = AbstractPage::factory([
            'type' => 'mvc'
        ]);

        $page->set('action', 'foo');
        $this->assertEquals('foo', $page->getAction());

        $page->set('Action', 'bar');
        $this->assertEquals('bar', $page->getAction());
    }

    public function testGetShouldMapToNativeProperties()
    {
        $page = AbstractPage::factory([
            'type' => 'mvc'
        ]);

        $page->setAction('foo');
        $this->assertEquals('foo', $page->get('action'));

        $page->setAction('bar');
        $this->assertEquals('bar', $page->get('Action'));
    }

    public function testShouldSetAndGetShouldMapToProperties()
    {
        $page = AbstractPage::factory([
            'type' => 'uri'
        ]);

        $page->set('action', 'Laughing Out Loud');
        $this->assertEquals('Laughing Out Loud', $page->get('action'));
    }

    public function testSetShouldNotMapToSetOptionsToPreventRecursion()
    {
        $page = AbstractPage::factory([
            'type' => 'uri',
            'label' => 'foo'
        ]);

        $options = ['label' => 'bar'];
        $page->set('options', $options);

        $this->assertEquals('foo', $page->getLabel());
        $this->assertEquals($options, $page->get('options'));
    }

    public function testSetShouldNotMapToSetConfigToPreventRecursion()
    {
        $page = AbstractPage::factory([
            'type' => 'uri',
            'label' => 'foo'
        ]);

        $options = ['label' => 'bar'];
        $page->set('config', $options);

        $this->assertEquals('foo', $page->getLabel());
        $this->assertEquals($options, $page->get('config'));
    }

    public function testSetShouldThrowExceptionIfPropertyIsNotString()
    {
        $page = AbstractPage::factory([
            'type' => 'uri',
        ]);

        $this->expectException(Exception\InvalidArgumentException::class);
        $page->set([], true);
    }

    public function testSetShouldThrowExceptionIfPropertyIsEmpty()
    {
        $page = AbstractPage::factory([
            'type' => 'uri',
        ]);

        $this->expectException(Exception\InvalidArgumentException::class);
        $page->set('', true);
    }

    public function testGetShouldThrowExceptionIfPropertyIsNotString()
    {
        $page = AbstractPage::factory([
            'type' => 'uri',
        ]);

        $this->expectException(Exception\InvalidArgumentException::class);
        $page->get([]);
    }

    public function testGetShouldThrowExceptionIfPropertyIsEmpty()
    {
        $page = AbstractPage::factory([
            'type' => 'uri',
        ]);

        $this->expectException(Exception\InvalidArgumentException::class);
        $page->get('');
    }

    public function testSetAndGetLabel()
    {
        $page = AbstractPage::factory([
            'label' => 'foo',
            'uri' => '#'
        ]);

        $this->assertEquals('foo', $page->getLabel());
        $page->setLabel('bar');
        $this->assertEquals('bar', $page->getLabel());

        $invalids = [42, (object) null];
        foreach ($invalids as $invalid) {
            try {
                $page->setLabel($invalid);
                $this->fail('An invalid value was set, but a ' .
                        'Zend\Navigation\Exception\InvalidArgumentException was not thrown');
            } catch (Navigation\Exception\InvalidArgumentException $e) {
                $this->assertContains('Invalid argument: $label', $e->getMessage());
            }
        }
    }

    /**
     * @group ZF-8922
     */
    public function testSetAndGetFragmentIdentifier()
    {
        $page = AbstractPage::factory([
            'uri'                => '#',
            'fragment' => 'foo',
        ]);

        $this->assertEquals('foo', $page->getFragment());

        $page->setFragment('bar');
        $this->assertEquals('bar', $page->getFragment());

        $invalids = [42, (object) null];
        foreach ($invalids as $invalid) {
            try {
                $page->setFragment($invalid);
                $this->fail('An invalid value was set, but a ' .
                            'Zend\Navigation\Exception\InvalidArgumentException was not thrown');
            } catch (Navigation\Exception\InvalidArgumentException $e) {
                $this->assertContains(
                    'Invalid argument: $fragment',
                    $e->getMessage()
                );
            }
        }
    }

    public function testSetAndGetId()
    {
        $page = AbstractPage::factory([
            'label' => 'foo',
            'uri' => '#'
        ]);

        $this->assertEquals(null, $page->getId());

        $page->setId('bar');
        $this->assertEquals('bar', $page->getId());

        $invalids = [true, (object) null];
        foreach ($invalids as $invalid) {
            try {
                $page->setId($invalid);
                $this->fail('An invalid value was set, but a ' .
                        'Zend\Navigation\Exception\InvalidArgumentException was not thrown');
            } catch (Navigation\Exception\InvalidArgumentException $e) {
                $this->assertContains('Invalid argument: $id', $e->getMessage());
            }
        }
    }

    public function testIdCouldBeAnInteger()
    {
        $page = AbstractPage::factory([
            'label' => 'foo',
            'uri' => '#',
            'id' => 10
        ]);

        $this->assertEquals(10, $page->getId());
    }

    public function testSetAndGetClass()
    {
        $page = AbstractPage::factory([
            'label' => 'foo',
            'uri' => '#'
        ]);

        $this->assertEquals(null, $page->getClass());
        $page->setClass('bar');
        $this->assertEquals('bar', $page->getClass());

        $invalids = [42, true, (object) null];
        foreach ($invalids as $invalid) {
            try {
                $page->setClass($invalid);
                $this->fail('An invalid value was set, but a ' .
                        'Zend\Navigation\Exception\InvalidArgumentException was not thrown');
            } catch (Navigation\Exception\InvalidArgumentException $e) {
                $this->assertContains('Invalid argument: $class', $e->getMessage());
            }
        }
    }

    public function testSetAndGetTitle()
    {
        $page = AbstractPage::factory([
            'label' => 'foo',
            'uri' => '#'
        ]);

        $this->assertEquals(null, $page->getTitle());
        $page->setTitle('bar');
        $this->assertEquals('bar', $page->getTitle());

        $invalids = [42, true, (object) null];
        foreach ($invalids as $invalid) {
            try {
                $page->setTitle($invalid);
                $this->fail('An invalid value was set, but a ' .
                        'Zend\Navigation\Exception\InvalidArgumentException was not thrown');
            } catch (Navigation\Exception\InvalidArgumentException $e) {
                $this->assertContains('Invalid argument: $title', $e->getMessage());
            }
        }
    }

    public function testSetAndGetTarget()
    {
        $page = AbstractPage::factory([
            'label' => 'foo',
            'uri' => '#'
        ]);

        $this->assertEquals(null, $page->getTarget());
        $page->setTarget('bar');
        $this->assertEquals('bar', $page->getTarget());

        $invalids = [42, true, (object) null];
        foreach ($invalids as $invalid) {
            try {
                $page->setTarget($invalid);
                $this->fail('An invalid value was set, but a ' .
                        'Zend\Navigation\Exception\InvalidArgumentException was not thrown');
            } catch (Navigation\Exception\InvalidArgumentException $e) {
                $this->assertContains('Invalid argument: $target', $e->getMessage());
            }
        }
    }

    public function testConstructingWithRelationsInArray()
    {
        $page = AbstractPage::factory([
            'label' => 'bar',
            'uri'   => '#',
            'rel'   => [
                'prev' => 'foo',
                'next' => 'baz'
            ],
            'rev'   => [
                'alternate' => 'bat'
            ]
        ]);

        $expected = [
            'rel'   => [
                'prev' => 'foo',
                'next' => 'baz'
            ],
            'rev'   => [
                'alternate' => 'bat'
            ]
        ];

        $actual = [
            'rel' => $page->getRel(),
            'rev' => $page->getRev()
        ];

        $this->assertEquals($expected, $actual);
    }

    public function testConstructingWithRelationsInConfig()
    {
        $page = AbstractPage::factory(new Config\Config([
            'label' => 'bar',
            'uri'   => '#',
            'rel'   => [
                'prev' => 'foo',
                'next' => 'baz'
            ],
            'rev'   => [
                'alternate' => 'bat'
            ]
        ]));

        $expected = [
            'rel'   => [
                'prev' => 'foo',
                'next' => 'baz'
            ],
            'rev'   => [
                'alternate' => 'bat'
            ]
        ];

        $actual = [
            'rel' => $page->getRel(),
            'rev' => $page->getRev()
        ];

        $this->assertEquals($expected, $actual);
    }

    public function testConstructingWithTraversableOptions()
    {
        $options = ['label' => 'bar'];

        $page = new Uri(new Config\Config($options));

        $actual = ['label' => $page->getLabel()];

        $this->assertEquals($options, $actual);
    }

    public function testGettingSpecificRelations()
    {
        $page = AbstractPage::factory([
            'label' => 'bar',
            'uri'   => '#',
            'rel'   => [
                'prev' => 'foo',
                'next' => 'baz'
            ],
            'rev'   => [
                'next' => 'foo'
            ]
        ]);

        $expected = [
            'foo', 'foo'
        ];

        $actual = [
            $page->getRel('prev'),
            $page->getRev('next')
        ];

        $this->assertEquals($expected, $actual);
    }

    public function testSetAndGetOrder()
    {
        $page = AbstractPage::factory([
            'label' => 'foo',
            'uri' => '#'
        ]);

        $this->assertEquals(null, $page->getOrder());

        $page->setOrder('1');
        $this->assertEquals(1, $page->getOrder());

        $page->setOrder(1337);
        $this->assertEquals(1337, $page->getOrder());

        $page->setOrder('-25');
        $this->assertEquals(-25, $page->getOrder());

        $invalids = [3.14, 'e', "\n", '0,4', true, (object) null];
        foreach ($invalids as $invalid) {
            try {
                $page->setOrder($invalid);
                $this->fail('An invalid value was set, but a ' .
                        'Zend\Navigation\Exception\InvalidArgumentException was not thrown');
            } catch (Navigation\Exception\InvalidArgumentException $e) {
                $this->assertContains('Invalid argument: $order', $e->getMessage());
            }
        }
    }

    public function testSetResourceString()
    {
        $page = AbstractPage::factory([
            'type'  => 'uri',
            'label' => 'hello'
        ]);

        $page->setResource('foo');
        $this->assertEquals('foo', $page->getResource());
    }

    public function testSetResourceNoParam()
    {
        $page = AbstractPage::factory([
            'type'     => 'uri',
            'label'    => 'hello',
            'resource' => 'foo'
        ]);

        $page->setResource();
        $this->assertEquals(null, $page->getResource());
    }

    public function testSetResourceNull()
    {
        $page = AbstractPage::factory([
            'type'     => 'uri',
            'label'    => 'hello',
            'resource' => 'foo'
        ]);

        $page->setResource(null);
        $this->assertEquals(null, $page->getResource());
    }

    public function testSetResourceInterface()
    {
        $page = AbstractPage::factory([
            'type'     => 'uri',
            'label'    => 'hello'
        ]);

        $resource = new \Zend\Permissions\Acl\Resource\GenericResource('bar');

        $page->setResource($resource);
        $this->assertEquals($resource, $page->getResource());
    }

    public function testSetResourceShouldThrowExceptionWhenGivenInteger()
    {
        $page = AbstractPage::factory([
            'type'     => 'uri',
            'label'    => 'hello'
        ]);

        try {
            $page->setResource(0);
            $this->fail('An invalid value was set, but a ' .
                        'Zend\Navigation\Exception\InvalidArgumentException was not thrown');
        } catch (Navigation\Exception\InvalidArgumentException $e) {
            $this->assertContains('Invalid argument: $resource', $e->getMessage());
        }
    }

    public function testSetResourceShouldThrowExceptionWhenGivenObject()
    {
        $page = AbstractPage::factory([
            'type'     => 'uri',
            'label'    => 'hello'
        ]);

        try {
            $page->setResource(new \stdClass());
            $this->fail('An invalid value was set, but a ' .
                        'Zend\Navigation\Exception\InvalidArgumentException was not thrown');
        } catch (Navigation\Exception\InvalidArgumentException $e) {
            $this->assertContains('Invalid argument: $resource', $e->getMessage());
        }
    }

    public function testSetPrivilegeNoParams()
    {
        $page = AbstractPage::factory([
            'type'     => 'uri',
            'label'    => 'hello',
            'privilege' => 'foo'
        ]);

        $page->setPrivilege();
        $this->assertEquals(null, $page->getPrivilege());
    }

    public function testSetPrivilegeNull()
    {
        $page = AbstractPage::factory([
            'type'     => 'uri',
            'label'    => 'hello',
            'privilege' => 'foo'
        ]);

        $page->setPrivilege(null);
        $this->assertEquals(null, $page->getPrivilege());
    }

    public function testSetPrivilegeString()
    {
        $page = AbstractPage::factory([
            'type'     => 'uri',
            'label'    => 'hello',
            'privilege' => 'foo'
        ]);

        $page->setPrivilege('bar');
        $this->assertEquals('bar', $page->getPrivilege());
    }

    public function testGetActiveOnNewlyConstructedPageShouldReturnFalse()
    {
        $page = new Uri();
        $this->assertFalse($page->getActive());
    }

    public function testIsActiveOnNewlyConstructedPageShouldReturnFalse()
    {
        $page = new Uri();
        $this->assertFalse($page->isActive());
    }

    public function testIsActiveRecursiveOnNewlyConstructedPageShouldReturnFalse()
    {
        $page = new Uri();
        $this->assertFalse($page->isActive(true));
    }

    public function testGetActiveShouldReturnTrueIfPageIsActive()
    {
        $page = new Uri(['active' => true]);
        $this->assertTrue($page->getActive());
    }

    public function testIsActiveShouldReturnTrueIfPageIsActive()
    {
        $page = new Uri(['active' => true]);
        $this->assertTrue($page->isActive());
    }

    public function testIsActiveWithRecursiveTrueShouldReturnTrueIfChildActive()
    {
        $page = new Uri([
            'label'  => 'Page 1',
            'active' => false,
            'pages'  => [
                new Uri([
                    'label'  => 'Page 1.1',
                    'active' => false,
                    'pages'  => [
                        new Uri([
                            'label'  => 'Page 1.1',
                            'active' => true
                        ])
                    ]
                ])
            ]
        ]);

        $this->assertFalse($page->isActive(false));
        $this->assertTrue($page->isActive(true));
    }

    public function testGetActiveWithRecursiveTrueShouldReturnTrueIfChildActive()
    {
        $page = new Uri([
            'label'  => 'Page 1',
            'active' => false,
            'pages'  => [
                new Uri([
                    'label'  => 'Page 1.1',
                    'active' => false,
                    'pages'  => [
                        new Uri([
                            'label'  => 'Page 1.1',
                            'active' => true
                        ])
                    ]
                ])
            ]
        ]);

        $this->assertFalse($page->getActive(false));
        $this->assertTrue($page->getActive(true));
    }

    public function testSetActiveWithNoParamShouldSetFalse()
    {
        $page = new Uri();
        $page->setActive();
        $this->assertTrue($page->getActive());
    }

    public function testSetActiveShouldJuggleValue()
    {
        $page = new Uri();

        $page->setActive(1);
        $this->assertTrue($page->getActive());

        $page->setActive('true');
        $this->assertTrue($page->getActive());

        $page->setActive(0);
        $this->assertFalse($page->getActive());

        $page->setActive([]);
        $this->assertFalse($page->getActive());
    }

    public function testIsVisibleOnNewlyConstructedPageShouldReturnTrue()
    {
        $page = new Uri();
        $this->assertTrue($page->isVisible());
    }

    public function testGetVisibleOnNewlyConstructedPageShouldReturnTrue()
    {
        $page = new Uri();
        $this->assertTrue($page->getVisible());
    }

    public function testIsVisibleShouldReturnFalseIfPageIsNotVisible()
    {
        $page = new Uri(['visible' => false]);
        $this->assertFalse($page->isVisible());
    }

    public function testGetVisibleShouldReturnFalseIfPageIsNotVisible()
    {
        $page = new Uri(['visible' => false]);
        $this->assertFalse($page->getVisible());
    }

    public function testIsVisibleRecursiveTrueShouldReturnFalseIfParentInivisble()
    {
        $page = new Uri([
            'label'  => 'Page 1',
            'visible' => false,
            'pages'  => [
                new Uri([
                    'label'  => 'Page 1.1',
                    'pages'  => [
                        new Uri([
                            'label'  => 'Page 1.1'
                        ])
                    ]
                ])
            ]
        ]);

        $childPage = $page->findOneByLabel('Page 1.1');
        $this->assertTrue($childPage->isVisible(false));
        $this->assertFalse($childPage->isVisible(true));
    }

    public function testGetVisibleRecursiveTrueShouldReturnFalseIfParentInivisble()
    {
        $page = new Uri([
            'label'  => 'Page 1',
            'visible' => false,
            'pages'  => [
                new Uri([
                    'label'  => 'Page 1.1',
                    'pages'  => [
                        new Uri([
                            'label'  => 'Page 1.1'
                        ])
                    ]
                ])
            ]
        ]);

        $childPage = $page->findOneByLabel('Page 1.1');
        $this->assertTrue($childPage->getVisible(false));
        $this->assertFalse($childPage->getVisible(true));
    }

    public function testSetVisibleWithNoParamShouldSetVisble()
    {
        $page = new Uri(['visible' => false]);
        $page->setVisible();
        $this->assertTrue($page->isVisible());
    }

    public function testSetVisibleShouldJuggleValue()
    {
        $page = new Uri();

        $page->setVisible(1);
        $this->assertTrue($page->isVisible());

        $page->setVisible('true');
        $this->assertTrue($page->isVisible());

        $page->setVisible(0);
        $this->assertFalse($page->isVisible());

        /**
         * ZF-10146
         *
         * @link http://framework.zend.com/issues/browse/ZF-10146
         */
        $page->setVisible('False');
        $this->assertFalse($page->isVisible());

        $page->setVisible([]);
        $this->assertFalse($page->isVisible());
    }

    public function testSetTranslatorTextDomainString()
    {
        $page = AbstractPage::factory([
            'type'  => 'uri',
            'label' => 'hello'
        ]);

        $page->setTextdomain('foo');
        $this->assertEquals('foo', $page->getTextdomain());
    }

    public function testMagicOverLoadsShouldSetAndGetNativeProperties()
    {
        $page = AbstractPage::factory([
            'label' => 'foo',
            'uri' => 'foo'
        ]);

        $this->assertSame('foo', $page->getUri());
        $this->assertSame('foo', $page->uri);

        $page->uri = 'bar';
        $this->assertSame('bar', $page->getUri());
        $this->assertSame('bar', $page->uri);
    }

    public function testMagicOverLoadsShouldCheckNativeProperties()
    {
        $page = AbstractPage::factory([
            'label' => 'foo',
            'uri' => 'foo'
        ]);

        $this->assertTrue(isset($page->uri));

        try {
            unset($page->uri);
            $this->fail('Should not be possible to unset native properties');
        } catch (Navigation\Exception\InvalidArgumentException $e) {
            $this->assertContains('Unsetting native property', $e->getMessage());
        }
    }

    public function testMagicOverLoadsShouldHandleCustomProperties()
    {
        $page = AbstractPage::factory([
            'label' => 'foo',
            'uri' => 'foo'
        ]);

        $this->assertFalse(isset($page->category));

        $page->category = 'music';
        $this->assertTrue(isset($page->category));
        $this->assertSame('music', $page->category);

        unset($page->category);
        $this->assertFalse(isset($page->category));
    }

    public function testMagicToStringMethodShouldReturnLabel()
    {
        $page = AbstractPage::factory([
            'label' => 'foo',
            'uri' => '#'
        ]);

        $this->assertEquals('foo', (string) $page);
    }

    public function testSetOptionsShouldTranslateToAccessor()
    {
        $page = AbstractPage::factory([
            'label' => 'foo',
            'action' => 'index',
            'controller' => 'index'
        ]);

        $options = [
            'label' => 'bar',
            'action' => 'baz',
            'controller' => 'bat',
            'id' => 'foo-test'
        ];

        $page->setOptions($options);

        $expected = [
            'label'       => 'bar',
            'action'      => 'baz',
            'controller'  => 'bat',
            'id'          => 'foo-test'
        ];

        $actual = [
            'label'       => $page->getLabel(),
            'action'      => $page->getAction(),
            'controller'  => $page->getController(),
            'id'          => $page->getId()
        ];

        $this->assertEquals($expected, $actual);
    }

    public function testSetOptionsShouldSetCustomProperties()
    {
        $page = AbstractPage::factory([
            'label' => 'foo',
            'uri' => '#'
        ]);

        $options = [
            'test' => 'test',
            'meaning' => 42
        ];

        $page->setOptions($options);

        $actual = [
            'test' => $page->test,
            'meaning' => $page->meaning
        ];

        $this->assertEquals($options, $actual);
    }

    public function testAddingRelations()
    {
        $page = AbstractPage::factory([
            'label' => 'page',
            'uri'   => '#'
        ]);

        $page->addRel('alternate', 'foo');
        $page->addRev('alternate', 'bar');

        $expected = [
            'rel' => ['alternate' => 'foo'],
            'rev' => ['alternate' => 'bar']
        ];

        $actual = [
            'rel' => $page->getRel(),
            'rev' => $page->getRev()
        ];

        $this->assertEquals($expected, $actual);
    }

    public function testRemovingRelations()
    {
        $page = AbstractPage::factory([
            'label' => 'page',
            'uri'   => '#'
        ]);

        $page->addRel('alternate', 'foo');
        $page->addRev('alternate', 'bar');
        $page->removeRel('alternate');
        $page->removeRev('alternate');

        $expected = [
            'rel' => [],
            'rev' => []
        ];

        $actual = [
            'rel' => $page->getRel(),
            'rev' => $page->getRev()
        ];

        $this->assertEquals($expected, $actual);
    }

    public function testSetRelShouldWorkWithArray()
    {
        $page = AbstractPage::factory([
            'type' => 'uri',
            'rel'  => [
                'foo' => 'bar',
                'baz' => 'bat'
            ]
        ]);

        $value = ['alternate' => 'format/xml'];
        $page->setRel($value);
        $this->assertEquals($value, $page->getRel());
    }

    public function testSetRelShouldWorkWithConfig()
    {
        $page = AbstractPage::factory([
            'type' => 'uri',
            'rel'  => [
                'foo' => 'bar',
                'baz' => 'bat'
            ]
        ]);

        $value = ['alternate' => 'format/xml'];
        $page->setRel(new Config\Config($value));
        $this->assertEquals($value, $page->getRel());
    }

    public function testSetRelShouldWithNoParamsShouldResetRelations()
    {
        $page = AbstractPage::factory([
            'type' => 'uri',
            'rel'  => [
                'foo' => 'bar',
                'baz' => 'bat'
            ]
        ]);

        $value = [];
        $page->setRel();
        $this->assertEquals($value, $page->getRel());
    }

    public function testSetRelShouldThrowExceptionWhenNotNullOrArrayOrConfig()
    {
        $page = AbstractPage::factory(['type' => 'uri']);

        try {
            $page->setRel('alternate');
            $this->fail('An invalid value was set, but a ' .
                        'Zend\Navigation\Exception\InvalidArgumentException was not thrown');
        } catch (Navigation\Exception\InvalidArgumentException $e) {
            $this->assertContains('Invalid argument: $relations', $e->getMessage());
        }
    }

    public function testSetRevShouldWorkWithArray()
    {
        $page = AbstractPage::factory([
            'type' => 'uri',
            'rev'  => [
                'foo' => 'bar',
                'baz' => 'bat'
            ]
        ]);

        $value = ['alternate' => 'format/xml'];
        $page->setRev($value);
        $this->assertEquals($value, $page->getRev());
    }

    public function testSetRevShouldWorkWithConfig()
    {
        $page = AbstractPage::factory([
            'type' => 'uri',
            'rev'  => [
                'foo' => 'bar',
                'baz' => 'bat'
            ]
        ]);

        $value = ['alternate' => 'format/xml'];
        $page->setRev(new Config\Config($value));
        $this->assertEquals($value, $page->getRev());
    }

    public function testSetRevShouldWithNoParamsShouldResetRelations()
    {
        $page = AbstractPage::factory([
            'type' => 'uri',
            'rev'  => [
                'foo' => 'bar',
                'baz' => 'bat'
            ]
        ]);

        $value = [];
        $page->setRev();
        $this->assertEquals($value, $page->getRev());
    }

    public function testSetRevShouldThrowExceptionWhenNotNullOrArrayOrConfig()
    {
        $page = AbstractPage::factory(['type' => 'uri']);

        try {
            $page->setRev('alternate');
            $this->fail('An invalid value was set, but a ' .
                        'Zend\Navigation\Exception\InvalidArgumentException was not thrown');
        } catch (Navigation\Exception\InvalidArgumentException $e) {
            $this->assertContains('Invalid argument: $relations', $e->getMessage());
        }
    }

    public function testGetRelWithArgumentShouldRetrieveSpecificRelation()
    {
        $page = AbstractPage::factory([
            'type' => 'uri',
            'rel'  => [
                'foo' => 'bar'
            ]
        ]);

        $this->assertEquals('bar', $page->getRel('foo'));
    }

    public function testGetRevWithArgumentShouldRetrieveSpecificRelation()
    {
        $page = AbstractPage::factory([
            'type' => 'uri',
            'rev'  => [
                'foo' => 'bar'
            ]
        ]);

        $this->assertEquals('bar', $page->getRev('foo'));
    }

    public function testGetDefinedRel()
    {
        $page = AbstractPage::factory([
            'type' => 'uri',
            'rel'  => [
                'alternate' => 'foo',
                'foo' => 'bar'
            ]
        ]);

        $expected = ['alternate', 'foo'];
        $this->assertEquals($expected, $page->getDefinedRel());
    }

    public function testGetDefinedRev()
    {
        $page = AbstractPage::factory([
            'type' => 'uri',
            'rev'  => [
                'alternate' => 'foo',
                'foo' => 'bar'
            ]
        ]);

        $expected = ['alternate', 'foo'];
        $this->assertEquals($expected, $page->getDefinedRev());
    }

    public function testGetCustomProperties()
    {
        $page = AbstractPage::factory([
            'label' => 'foo',
            'uri' => '#',
            'baz' => 'bat'
        ]);

        $options = [
            'test' => 'test',
            'meaning' => 42
        ];

        $page->setOptions($options);

        $expected = [
            'baz' => 'bat',
            'test' => 'test',
            'meaning' => 42
        ];

        $this->assertEquals($expected, $page->getCustomProperties());
    }

    public function testToArrayMethod()
    {
        $options = [
            'label'    => 'foo',
            'uri'      => 'http://www.example.com/foo.html',
            'fragment' => 'bar',
            'id'       => 'my-id',
            'class'    => 'my-class',
            'title'    => 'my-title',
            'target'   => 'my-target',
            'rel'      => [],
            'rev'      => [],
            'order'    => 100,
            'active'   => true,
            'visible'  => false,

            'resource' => 'joker',
            'privilege' => null,
            'permission' => null,

            'foo'      => 'bar',
            'meaning'  => 42,

            'pages'    => [
                [
                    'type'      => 'Zend\Navigation\Page\Uri',
                    'label'     => 'foo.bar',
                    'fragment'  => null,
                    'id'        => null,
                    'class'     => null,
                    'title'     => null,
                    'target'    => null,
                    'rel'       => [],
                    'rev'       => [],
                    'order'     => null,
                    'resource'  => null,
                    'privilege' => null,
                    'permission' => null,
                    'active'    => null,
                    'visible'   => 1,
                    'pages'     => [],
                    'uri'       => 'http://www.example.com/foo.html',
                ],
                [
                    'label'     => 'foo.baz',
                    'type'      => 'Zend\Navigation\Page\Uri',
                    'label'     => 'foo.bar',
                    'fragment'  => null,
                    'id'        => null,
                    'class'     => null,
                    'title'     => null,
                    'target'    => null,
                    'rel'       => [],
                    'rev'       => [],
                    'order'     => null,
                    'resource'  => null,
                    'privilege' => null,
                    'permission' => null,
                    'active'    => null,
                    'visible'   => 1,
                    'pages'     => [],
                    'uri'       => 'http://www.example.com/foo.html'
                ]
            ]
        ];

        $page = AbstractPage::factory($options);
        $toArray = $page->toArray();

        // tweak options to what we expect toArray() to contain
        $options['type'] = 'Zend\Navigation\Page\Uri';

        ksort($options);
        ksort($toArray);
        $this->assertEquals($options, $toArray);
    }

    public function testSetPermission()
    {
        $page = AbstractPage::factory([
            'type'       => 'uri'
        ]);

        $page->setPermission('my_permission');
        $this->assertEquals('my_permission', $page->getPermission());
    }

    public function testSetArrayPermission()
    {
        $page = AbstractPage::factory([
            'type'       => 'uri'
        ]);

        $page->setPermission(['my_permission', 'other_permission']);
        $this->assertInternalType('array', $page->getPermission());
        $this->assertCount(2, $page->getPermission());
    }

    public function testSetObjectPermission()
    {
        $page = AbstractPage::factory([
            'type'       => 'uri'
        ]);

        $permission = new \stdClass();
        $permission->name = 'my_permission';

        $page->setPermission($permission);
        $this->assertInstanceOf('stdClass', $page->getPermission());
        $this->assertEquals('my_permission', $page->getPermission()->name);
    }

    public function testSetParentShouldThrowExceptionIfPageItselfIsParent()
    {
        $page = AbstractPage::factory(
            [
                'type' => 'uri',
            ]
        );

        $this->expectException(Exception\InvalidArgumentException::class);
        $page->setParent($page);
    }
}
