<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2015 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\View\Helper;

use PHPUnit\Framework\TestCase;
use Zend\Authentication\AuthenticationService;
use Zend\Authentication\Storage\NonPersistent as NonPersistentStorage;
use Zend\View\Helper\Identity as IdentityHelper;

/**
 * Zend_View_Helper_IdentityTest
 *
 * Tests Identity helper
 *
 * @group      Zend_View
 * @group      Zend_View_Helper
 */
class IdentityTest extends TestCase
{
    public function testGetIdentity()
    {
        $identity = new TestAsset\IdentityObject();
        $identity->setUsername('a username');
        $identity->setPassword('a password');

        $authenticationService = new AuthenticationService(
            new NonPersistentStorage,
            new TestAsset\AuthenticationAdapter
        );

        $identityHelper = new IdentityHelper;
        $identityHelper->setAuthenticationService($authenticationService);

        $this->assertNull($identityHelper());

        $this->assertFalse($authenticationService->hasIdentity());

        $authenticationService->getAdapter()->setIdentity($identity);
        $result = $authenticationService->authenticate();
        $this->assertTrue($result->isValid());

        $this->assertEquals($identity, $identityHelper());
    }
}
