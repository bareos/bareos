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
use Zend\Navigation\Page\AbstractPage;
use Zend\Navigation;

/**
 * Tests Zend_Navigation_Page::factory()
 *
 * @group      Zend_Navigation
 */
class PageFactoryTest extends TestCase
{
    public function testDetectFactoryPage()
    {
        AbstractPage::addFactory(function ($page) {
            if (isset($page['factory_uri'])) {
                return new \Zend\Navigation\Page\Uri($page);
            } elseif (isset($page['factory_mvc'])) {
                return new \Zend\Navigation\Page\Mvc($page);
            }
        });

        $this->assertInstanceOf('Zend\\Navigation\\Page\\Uri', AbstractPage::factory([
            'label' => 'URI Page',
            'factory_uri' => '#'
        ]));

        $this->assertInstanceOf('Zend\\Navigation\\Page\\Mvc', AbstractPage::factory([
            'label' => 'URI Page',
            'factory_mvc' => '#'
        ]));
    }

    public function testDetectMvcPage()
    {
        $pages = [
            AbstractPage::factory([
                'label' => 'MVC Page',
                'action' => 'index'
            ]),
            AbstractPage::factory([
                'label' => 'MVC Page',
                'controller' => 'index'
            ]),
            AbstractPage::factory([
                'label' => 'MVC Page',
                'route' => 'home'
            ])
        ];

        $this->assertContainsOnly('Zend\Navigation\Page\Mvc', $pages);
    }

    public function testDetectUriPage()
    {
        $page = AbstractPage::factory([
            'label' => 'URI Page',
            'uri' => '#'
        ]);

        $this->assertInstanceOf('Zend\\Navigation\\Page\\Uri', $page);
    }

    public function testMvcShouldHaveDetectionPrecedence()
    {
        $page = AbstractPage::factory([
            'label' => 'MVC Page',
            'action' => 'index',
            'controller' => 'index',
            'uri' => '#'
        ]);

        $this->assertInstanceOf('Zend\\Navigation\\Page\\Mvc', $page);
    }

    public function testSupportsMvcShorthand()
    {
        $mvcPage = AbstractPage::factory([
            'type' => 'mvc',
            'label' => 'MVC Page',
            'action' => 'index',
            'controller' => 'index'
        ]);

        $this->assertInstanceOf('Zend\\Navigation\\Page\\Mvc', $mvcPage);
    }

    public function testSupportsUriShorthand()
    {
        $uriPage = AbstractPage::factory([
            'type' => 'uri',
            'label' => 'URI Page',
            'uri' => 'http://www.example.com/'
        ]);

        $this->assertInstanceOf('Zend\\Navigation\\Page\\Uri', $uriPage);
    }

    public function testSupportsCustomPageTypes()
    {
        $page = AbstractPage::factory([
            'type' => 'ZendTest\Navigation\TestAsset\Page',
            'label' => 'My Custom Page'
        ]);

        return $this->assertInstanceOf('ZendTest\\Navigation\\TestAsset\\Page', $page);
    }

    public function testShouldFailForInvalidType()
    {
        $this->expectException(
            Navigation\Exception\InvalidArgumentException::class
        );

        AbstractPage::factory([
            'type' => 'ZendTest\Navigation\TestAsset\InvalidPage',
            'label' => 'My Invalid Page'
        ]);
    }

    public function testShouldFailForNonExistantType()
    {
        $this->expectException(
            Navigation\Exception\InvalidArgumentException::class
        );

        $pageConfig = [
            'type' => 'My_NonExistent_Page',
            'label' => 'My non-existent Page'
        ];

        AbstractPage::factory($pageConfig);
    }

    public function testShouldFailIfUnableToDetermineType()
    {
        $this->expectException(
            Navigation\Exception\InvalidArgumentException::class
        );

        AbstractPage::factory([
            'label' => 'My Invalid Page'
        ]);
    }

    /**
     * @expectedException \Zend\Navigation\Exception\InvalidArgumentException
     */
    public function testShouldThrowExceptionOnInvalidMethodArgument()
    {
        AbstractPage::factory('');
    }
}
