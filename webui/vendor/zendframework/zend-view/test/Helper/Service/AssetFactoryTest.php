<?php
/**
 * @see       https://github.com/zendframework/zend-view for the canonical source repository
 * @copyright Copyright (c) 2017 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   https://github.com/zendframework/zend-view/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\View\Helper\Service;

use PHPUnit\Framework\TestCase;
use Zend\ServiceManager\ServiceManager;
use Zend\View\Exception;
use Zend\View\Helper\Asset;
use Zend\View\Helper\Service\AssetFactory;
use Zend\View\HelperPluginManager;

class AssetFactoryTest extends TestCase
{
    public function testAssetFactoryCreateServiceCreatesAssetInstance()
    {
        $services = $this->getServices();

        $assetFactory = new AssetFactory();
        $asset = $assetFactory->createService($services);

        $this->assertInstanceOf(Asset::class, $asset);
    }

    public function testAssetFactoryInvokableCreatesAssetInstance()
    {
        $services = $this->getServices();

        $assetFactory = new AssetFactory();
        $asset = $assetFactory($services, '');

        $this->assertInstanceOf(Asset::class, $asset);
    }

    public function testValidConfiguration()
    {
        $config = [
            'view_helper_config' => [
                'asset' => [
                    'resource_map' => [
                        'css/style.css' => 'css/style-3a97ff4ee3.css',
                        'js/vendor.js' => 'js/vendor-a507086eba.js',
                    ],
                ],
            ],
        ];

        $services = $this->getServices($config);
        $assetFactory = new AssetFactory();

        $asset = $assetFactory($services, '');

        $this->assertEquals($config['view_helper_config']['asset']['resource_map'], $asset->getResourceMap());
    }

    public function testInvalidConfiguration()
    {
        $config = [
            'view_helper_config' => [
                'asset' => [],
            ],
        ];
        $services = $this->getServices($config);

        $assetFactory = new AssetFactory();

        $this->expectException(Exception\RuntimeException::class);
        $this->expectExceptionMessage('Invalid resource map configuration');
        $assetFactory($services, '');
    }

    protected function getServices(array $config = [])
    {
        $services = $this->prophesize(ServiceManager::class);
        $services->get('config')->willReturn($config);

        $helpers = new HelperPluginManager($services->reveal());

        // test if we are using Zend\ServiceManager v3
        if (method_exists($helpers, 'configure')) {
            return $services->reveal();
        }

        return $helpers;
    }
}
