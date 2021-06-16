<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2015 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\View\Helper\Navigation;

use Zend\Navigation\Navigation;
use Zend\View\Exception\ExceptionInterface;
use Zend\View\Helper\Navigation\Breadcrumbs;

/**
 * Tests Zend\View\Helper\Navigation\Breadcrumbs.
 *
 * @group      Zend_View
 * @group      Zend_View_Helper
 */
class BreadcrumbsTest extends AbstractTest
{
    // @codingStandardsIgnoreStart
    /**
     * Class name for view helper to test.
     *
     * @var string
     */
    protected $_helperName = Breadcrumbs::class;

    /**
     * View helper.
     *
     * @var Breadcrumbs
     */
    protected $_helper;
    // @codingStandardsIgnoreEnd

    public function testCanRenderStraightFromServiceAlias()
    {
        $this->_helper->setServiceLocator($this->serviceManager);

        $returned = $this->_helper->renderStraight('Navigation');
        $this->assertEquals($returned, $this->_getExpected('bc/default.html'));
    }

    public function testCanRenderPartialFromServiceAlias()
    {
        $this->_helper->setPartial('bc.phtml');
        $this->_helper->setServiceLocator($this->serviceManager);

        $returned = $this->_helper->renderPartial('Navigation');
        $this->assertEquals($returned, $this->_getExpected('bc/partial.html'));
    }

    public function testHelperEntryPointWithoutAnyParams()
    {
        $returned = $this->_helper->__invoke();
        $this->assertEquals($this->_helper, $returned);
        $this->assertEquals($this->_nav1, $returned->getContainer());
    }

    public function testHelperEntryPointWithContainerParam()
    {
        $returned = $this->_helper->__invoke($this->_nav2);
        $this->assertEquals($this->_helper, $returned);
        $this->assertEquals($this->_nav2, $returned->getContainer());
    }

    public function testHelperEntryPointWithContainerStringParam()
    {
        $pm = new \Zend\View\HelperPluginManager($this->serviceManager);
        $this->_helper->setServiceLocator($pm);

        $returned = $this->_helper->__invoke('nav1');
        $this->assertEquals($this->_helper, $returned);
        $this->assertEquals($this->_nav1, $returned->getContainer());
    }

    public function testNullOutContainer()
    {
        $old = $this->_helper->getContainer();
        $this->_helper->setContainer();
        $new = $this->_helper->getContainer();

        $this->assertNotEquals($old, $new);
    }

    public function testSetSeparator()
    {
        $this->_helper->setSeparator('foo');

        $expected = $this->_getExpected('bc/separator.html');
        $this->assertEquals($expected, $this->_helper->render());
    }

    public function testSetMaxDepth()
    {
        $this->_helper->setMaxDepth(1);

        $expected = $this->_getExpected('bc/maxdepth.html');
        $this->assertEquals($expected, $this->_helper->render());
    }

    public function testSetMinDepth()
    {
        $this->_helper->setMinDepth(1);

        $expected = '';
        $this->assertEquals($expected, $this->_helper->render($this->_nav2));
    }

    public function testLinkLastElement()
    {
        $this->_helper->setLinkLast(true);

        $expected = $this->_getExpected('bc/linklast.html');
        $this->assertEquals($expected, $this->_helper->render());
    }

    public function testSetIndent()
    {
        $this->_helper->setIndent(8);

        $expected = '        <a';
        $actual = substr($this->_helper->render(), 0, strlen($expected));

        $this->assertEquals($expected, $actual);
    }

    public function testRenderSuppliedContainerWithoutInterfering()
    {
        $this->_helper->setMinDepth(0);

        $rendered1 = $this->_getExpected('bc/default.html');
        $rendered2 = 'Site 2';

        $expected = [
            'registered'       => $rendered1,
            'supplied'         => $rendered2,
            'registered_again' => $rendered1,
        ];

        $actual = [
            'registered'       => $this->_helper->render(),
            'supplied'         => $this->_helper->render($this->_nav2),
            'registered_again' => $this->_helper->render(),
        ];

        $this->assertEquals($expected, $actual);
    }

    public function testUseAclResourceFromPages()
    {
        $acl = $this->_getAcl();
        $this->_helper->setAcl($acl['acl']);
        $this->_helper->setRole($acl['role']);

        $expected = $this->_getExpected('bc/acl.html');
        $this->assertEquals($expected, $this->_helper->render());
    }

    public function testTranslationUsingZendTranslate()
    {
        if (! extension_loaded('intl')) {
            $this->markTestSkipped('ext/intl not enabled');
        }

        $this->_helper->setTranslator($this->_getTranslator());

        $expected = $this->_getExpected('bc/translated.html');
        $this->assertEquals($expected, $this->_helper->render());
    }

    public function testTranslationUsingZendTranslateAndCustomTextDomain()
    {
        if (! extension_loaded('intl')) {
            $this->markTestSkipped('ext/intl not enabled');
        }

        $this->_helper->setTranslator($this->_getTranslatorWithTextDomain());

        $expected = $this->_getExpected('bc/textdomain.html');
        $test     = $this->_helper->render($this->_nav3);

        $this->assertEquals(trim($expected), trim($test));
    }

    public function testTranslationUsingZendTranslateAdapter()
    {
        if (! extension_loaded('intl')) {
            $this->markTestSkipped('ext/intl not enabled');
        }

        $translator = $this->_getTranslator();
        $this->_helper->setTranslator($translator);

        $expected = $this->_getExpected('bc/translated.html');
        $this->assertEquals($expected, $this->_helper->render());
    }

    public function testDisablingTranslation()
    {
        $translator = $this->_getTranslator();
        $this->_helper->setTranslator($translator);
        $this->_helper->setTranslatorEnabled(false);

        $expected = $this->_getExpected('bc/default.html');
        $this->assertEquals($expected, $this->_helper->render());
    }

    public function testRenderingPartial()
    {
        $this->_helper->setPartial('bc.phtml');

        $expected = $this->_getExpected('bc/partial.html');
        $this->assertEquals($expected, $this->_helper->render());
    }

    public function testRenderingPartialWithSeparator()
    {
        $this->_helper->setPartial('bc_separator.phtml')->setSeparator(' / ');

        $expected = trim($this->_getExpected('bc/partialwithseparator.html'));
        $this->assertEquals($expected, $this->_helper->render());
    }

    public function testRenderingPartialBySpecifyingAnArrayAsPartial()
    {
        $this->_helper->setPartial(['bc.phtml', 'application']);

        $expected = $this->_getExpected('bc/partial.html');
        $this->assertEquals($expected, $this->_helper->render());
    }

    public function testRenderingPartialShouldFailOnInvalidPartialArray()
    {
        $this->_helper->setPartial(['bc.phtml']);

        try {
            $this->_helper->render();
            $this->fail(
                '$partial was invalid, but no Zend\View\Exception\ExceptionInterface was thrown'
            );
        } catch (ExceptionInterface $e) {
        }
    }

    public function testRenderingPartialWithParams()
    {
        $this->_helper->setPartial('bc_with_partial_params.phtml')->setSeparator(' / ');
        $expected = $this->_getExpected('bc/partial_with_params.html');
        $actual = $this->_helper->renderPartialWithParams(['variable' => 'test value']);
        $this->assertEquals($expected, $actual);
    }

    public function testLastBreadcrumbShouldBeEscaped()
    {
        $container = new Navigation([
            [
                'label'  => 'Live & Learn',
                'uri'    => '#',
                'active' => true,
            ],
        ]);

        $expected = 'Live &amp; Learn';
        $actual = $this->_helper->setMinDepth(0)->render($container);

        $this->assertEquals($expected, $actual);
    }
}
