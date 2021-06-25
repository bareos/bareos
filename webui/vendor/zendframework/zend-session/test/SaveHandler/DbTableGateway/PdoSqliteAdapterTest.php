<?php
/**
 * @see       https://github.com/zendframework/zend-session for the canonical source repository
 * @copyright Copyright (c) 2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-session/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\Session\SaveHandler\DbTableGateway;

use Zend\Db\Adapter\Adapter;
use Zend\Session\SaveHandler\DbTableGateway;
use ZendTest\Session\SaveHandler\DbTableGatewayTest;

class PdoSqliteAdapterTest extends DbTableGatewayTest
{
    /**
     * @return Adapter
     */
    protected function getAdapter()
    {
        if (! extension_loaded('pdo_sqlite')) {
            $this->markTestSkipped(sprintf(
                '%s tests with PDO_Sqlite adapter are not enabled due to missing PDO_Sqlite extension',
                DbTableGateway::class
            ));
        }

        return new Adapter([
            'driver' => 'pdo_sqlite',
            'database' => ':memory:',
        ]);
    }
}
