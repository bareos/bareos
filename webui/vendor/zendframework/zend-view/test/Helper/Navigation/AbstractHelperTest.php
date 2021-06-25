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
use Zend\ServiceManager\ServiceManager;
use Zend\View\Helper\Navigation as NavigationHelper;

class AbstractHelperTest extends AbstractTest
{
    // @codingStandardsIgnoreStart
    /**
     * Class name for view helper to test
     *
     * @var string
     */
    protected $_helperName = NavigationHelper::class;

    /**
     * View helper
     *
     * @var NavigationHelper\Breadcrumbs
     */
    protected $_helper;
    // @codingStandardsIgnoreEnd

    protected function tearDown()
    {
        parent::tearDown();

        if ($this->_helper) {
            $this->_helper->setDefaultAcl(null);
            $this->_helper->setAcl(null);
            $this->_helper->setDefaultRole(null);
            $this->_helper->setRole(null);
        }
    }

    public function testHasACLChecksDefaultACL()
    {
        $aclContainer = $this->_getAcl();
        $acl = $aclContainer['acl'];

        $this->assertEquals(false, $this->_helper->hasACL());
        $this->_helper->setDefaultAcl($acl);
        $this->assertEquals(true, $this->_helper->hasAcl());
    }

    public function testHasACLChecksMemberVariable()
    {
        $aclContainer = $this->_getAcl();
        $acl = $aclContainer['acl'];

        $this->assertEquals(false, $this->_helper->hasAcl());
        $this->_helper->setAcl($acl);
        $this->assertEquals(true, $this->_helper->hasAcl());
    }

    public function testHasRoleChecksDefaultRole()
    {
        $aclContainer = $this->_getAcl();
        $role = $aclContainer['role'];

        $this->assertEquals(false, $this->_helper->hasRole());
        $this->_helper->setDefaultRole($role);
        $this->assertEquals(true, $this->_helper->hasRole());
    }

    public function testHasRoleChecksMemberVariable()
    {
        $aclContainer = $this->_getAcl();
        $role = $aclContainer['role'];

        $this->assertEquals(false, $this->_helper->hasRole());
        $this->_helper->setRole($role);
        $this->assertEquals(true, $this->_helper->hasRole());
    }

    public function testEventManagerIsNullByDefault()
    {
        $this->assertNull($this->_helper->getEventManager());
    }

    public function testFallBackForContainerNames()
    {
        // Register navigation service with name equal to the documentation
        $this->serviceManager->setAllowOverride(true);
        $this->serviceManager->setService(
            'navigation',
            $this->serviceManager->get('Navigation')
        );
        $this->serviceManager->setAllowOverride(false);

        $this->_helper->setServiceLocator($this->serviceManager);

        $this->_helper->setContainer('navigation');
        $this->assertInstanceOf(
            Navigation::class,
            $this->_helper->getContainer()
        );

        $this->_helper->setContainer('default');
        $this->assertInstanceOf(
            Navigation::class,
            $this->_helper->getContainer()
        );
    }
}
