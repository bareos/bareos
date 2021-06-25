<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2015 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Session\Service;

use PHPUnit\Framework\TestCase;
use Zend\EventManager\Test\EventListenerIntrospectionTrait;
use Zend\ServiceManager\Config;
use Zend\ServiceManager\ServiceManager;
use Zend\ServiceManager\Exception\ServiceNotCreatedException;
use Zend\Session\Config\ConfigInterface;
use Zend\Session\Container;
use Zend\Session\ManagerInterface;
use Zend\Session\SaveHandler\SaveHandlerInterface;
use Zend\Session\Service\SessionManagerFactory;
use Zend\Session\SessionManager;
use Zend\Session\Storage\ArrayStorage;
use Zend\Session\Storage\StorageInterface;
use Zend\Session\Validator;
use ZendTest\Session\TestAsset\TestManager;
use ZendTest\Session\TestAsset\TestSaveHandler;

/**
 * @group      Zend_Session
 * @covers Zend\Session\Service\SessionManagerFactory
 */
class SessionManagerFactoryTest extends TestCase
{
    use EventListenerIntrospectionTrait;

    public function setUp()
    {
        $config = new Config([
            'factories' => [
                ManagerInterface::class => SessionManagerFactory::class,
                TestManager::class => SessionManagerFactory::class,
                TestSaveHandler::class => SessionManagerFactory::class,
            ],
        ]);
        $this->services = new ServiceManager();
        $config->configureServiceManager($this->services);
    }

    public function testCreatesSessionManager()
    {
        $manager = $this->services->get(ManagerInterface::class);
        $this->assertInstanceOf(SessionManager::class, $manager);
    }

    public function testConfigObjectIsInjectedIfPresentInServices()
    {
        $config = $this->createMock(ConfigInterface::class);
        $this->services->setService(ConfigInterface::class, $config);
        $manager = $this->services->get(ManagerInterface::class);
        $test = $manager->getConfig();
        $this->assertSame($config, $test);
    }

    public function testFactoryWillInjectStorageIfPresentInServices()
    {
        // Using concrete version here as mocking was too complex
        $storage = new ArrayStorage();
        $this->services->setService(StorageInterface::class, $storage);
        $manager = $this->services->get(ManagerInterface::class);
        $test = $manager->getStorage();
        $this->assertSame($storage, $test);
    }

    public function testFactoryWillInjectSaveHandlerIfPresentInServices()
    {
        $saveHandler = $this->createMock(SaveHandlerInterface::class);
        $this->services->setService(SaveHandlerInterface::class, $saveHandler);
        $manager = $this->services->get(ManagerInterface::class);
        $test = $manager->getSaveHandler();
        $this->assertSame($saveHandler, $test);
    }

    public function testFactoryWillMarkManagerAsContainerDefaultByDefault()
    {
        $manager = $this->services->get(ManagerInterface::class);
        $this->assertSame($manager, Container::getDefaultManager());
    }

    public function testCanDisableContainerDefaultManagerInjectionViaConfiguration()
    {
        $config = ['session_manager' => [
            'enable_default_container_manager' => false,
        ]];
        $this->services->setService('config', $config);
        $manager = $this->services->get(ManagerInterface::class);
        $this->assertNotSame($manager, Container::getDefaultManager());
    }

    /**
     * @runInSeparateProcess
     */
    public function testFactoryWillAddValidatorViaConfiguration()
    {
        $config = ['session_manager' => [
            'validators' => [
                Validator\RemoteAddr::class,
            ],
        ]];
        $this->services->setService('config', $config);
        $manager = $this->services->get(ManagerInterface::class);

        $manager->start();

        $chain = $manager->getValidatorChain();
        $listeners = iterator_to_array($this->getListenersForEvent('session.validate', $chain));
        $this->assertCount(2, $listeners);
    }

    /**
     * @runInSeparateProcess
     */
    public function testStartingSessionManagerFromFactoryDoesNotTriggerUndefinedVariable()
    {
        $storage = new ArrayStorage();
        $this->services->setService(StorageInterface::class, $storage);

        $manager = $this->services->get(ManagerInterface::class);
        $manager->start();

        $this->assertSame($storage, $manager->getStorage());
    }

    /**
     * @runInSeparateProcess
     */
    public function testFactoryDoesNotOverwriteValidatorStorageValues()
    {
        $storage = new ArrayStorage();
        $storage->setMetadata('_VALID', [
            Validator\HttpUserAgent::class => 'Foo',
            Validator\RemoteAddr::class    => '1.2.3.4',
        ]);
        $this->services->setService(StorageInterface::class, $storage);
        $this->services->setService('config', [
            'session_manager' => [
                'validators' => [
                    Validator\HttpUserAgent::class,
                    Validator\RemoteAddr::class,
                ],
            ],
        ]);

        // This call is needed to make sure session storage data is not overwritten by the factory
        $manager = $this->services->get(ManagerInterface::class);

        $validatorData = $storage->getMetaData('_VALID');
        $this->assertSame('Foo', $validatorData[Validator\HttpUserAgent::class]);
        $this->assertSame('1.2.3.4', $validatorData[Validator\RemoteAddr::class]);
    }

    /**
     * @runInSeparateProcess
     */
    public function testFactoryDoesNotAttachValidatorTwoTimes()
    {
        $storage = new ArrayStorage();
        $storage->setMetadata('_VALID', [
            Validator\RemoteAddr::class => '1.2.3.4',
        ]);
        $this->services->setService(StorageInterface::class, $storage);
        $this->services->setService('config', [
            'session_manager' => [
                'validators' => [
                    Validator\RemoteAddr::class,
                ],
            ],
        ]);

        $manager = $this->services->get(ManagerInterface::class);
        try {
            $manager->start();
        } catch (\RuntimeException $e) {
            // Ignore exception, because we are not interested whether session validation passes in this test
        }

        $chain = $manager->getValidatorChain();
        $listeners = iterator_to_array($this->getListenersForEvent('session.validate', $chain));
        $this->assertCount(2, $listeners);

        $found = false;
        foreach ($listeners as $listener) {
            // Listeners are all [$validator, 'isValid'] callbacks
            if ($listener[0] instanceof Validator\RemoteAddr) {
                $found = true;
                break;
            }
        }
        $this->assertTrue($found, 'Did not find RemoteAddr validator in listeners');
    }

    public function testFactoryAllowsOverridingOptions()
    {
        $storage = new ArrayStorage();
        $this->services->setService(StorageInterface::class, $storage);
        $this->services->setService('config', [
            'session_manager' => [
                'options' => [
                    'attach_default_validators' => false,
                ],
            ],
        ]);

        $manager = $this->services->get(ManagerInterface::class);
        $this->assertAttributeSame([], 'validators', $manager);
    }

    public function testFactoryWillUseRequestedNameAsSessionManagerIfItImplementsManagerInterface()
    {
        $manager = $this->services->get(TestManager::class);
        $this->assertInstanceOf(TestManager::class, $manager);
    }

    public function testFactoryWillRaiseServiceNotCreatedExceptionIfRequestedNameIsNotAManagerInterfaceSubclass()
    {
        $this->expectException(ServiceNotCreatedException::class);
        $manager = $this->services->get(TestSaveHandler::class);
    }
}
