<?php
/**
 * @see       https://github.com/zendframework/zend-filter for the canonical source repository
 * @copyright Copyright (c) 20052018 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-filter/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\Filter\File;

use PHPUnit\Framework\TestCase;
use Prophecy\Argument;
use Psr\Http\Message\StreamFactoryInterface;
use Psr\Http\Message\StreamInterface;
use Psr\Http\Message\UploadedFileFactoryInterface;
use Psr\Http\Message\UploadedFileInterface;
use Zend\Filter\Exception;
use Zend\Filter\File\RenameUpload as FileRenameUpload;

class RenameUploadTest extends TestCase
{
    /**
     * Path to test files
     *
     * @var string
     */
    protected $filesPath;

    /**
     * Testfile
     *
     * @var string
     */
    protected $sourceFile;

    /**
     * Testfile
     *
     * @var string
     */
    protected $targetFile;

    /**
     * Testdirectory
     *
     * @var string
     */
    protected $targetPath;

    /**
     * Testfile in Testdirectory
     *
     * @var string
     */
    protected $targetPathFile;

    /**
     * Sets the path to test files
     *
     * @return void
     */
    public function setUp()
    {
        $this->filesPath = sprintf('%s%s%s', sys_get_temp_dir(), DIRECTORY_SEPARATOR, uniqid('zfilter'));
        $this->targetPath = sprintf('%s%s%s', $this->filesPath, DIRECTORY_SEPARATOR, 'targetPath');

        mkdir($this->targetPath, 0775, true);

        $this->sourceFile = $this->filesPath . DIRECTORY_SEPARATOR . 'testfile.txt';
        $this->targetFile = $this->filesPath . DIRECTORY_SEPARATOR . 'newfile.xml';
        $this->targetPathFile = $this->targetPath . DIRECTORY_SEPARATOR . 'testfile.txt';

        touch($this->sourceFile);
    }

    /**
     * Sets the path to test files
     *
     * @return void
     */
    public function tearDown()
    {
        $this->removeDir($this->filesPath);
    }

    protected function removeDir($dir)
    {
        if (! is_dir($dir)) {
            return;
        }

        foreach (glob($dir . DIRECTORY_SEPARATOR . '*') as $file) {
            if (is_file($file)) {
                unlink($file);
                continue;
            }
            if (is_dir($file)) {
                $this->removeDir($file);
                continue;
            }
        }

        rmdir($dir);
    }

    /**
     * Test single parameter filter
     *
     * @return void
     */
    public function testThrowsExceptionWithNonUploadedFile()
    {
        $filter = new FileRenameUpload($this->targetFile);
        $this->assertEquals($this->targetFile, $filter->getTarget());
        $this->assertEquals('falsefile', $filter('falsefile'));
        $this->expectException(Exception\RuntimeException::class);
        $this->expectExceptionMessage('could not be renamed');
        $this->assertEquals($this->targetFile, $filter($this->sourceFile));
    }

    /**
     * @return void
     */
    public function testOptions()
    {
        $filter = new FileRenameUpload($this->targetFile);
        $this->assertEquals($this->targetFile, $filter->getTarget());
        $this->assertFalse($filter->getUseUploadName());
        $this->assertFalse($filter->getOverwrite());
        $this->assertFalse($filter->getRandomize());

        $filter = new FileRenameUpload([
            'target'          => $this->sourceFile,
            'use_upload_name' => true,
            'overwrite'       => true,
            'randomize'       => true,
        ]);
        $this->assertEquals($this->sourceFile, $filter->getTarget());
        $this->assertTrue($filter->getUseUploadName());
        $this->assertTrue($filter->getOverwrite());
        $this->assertTrue($filter->getRandomize());
    }

    /**
     * @return void
     */
    public function testStringConstructorParam()
    {
        $filter = new RenameUploadMock($this->targetFile);
        $this->assertEquals($this->targetFile, $filter->getTarget());
        $this->assertEquals($this->targetFile, $filter($this->sourceFile));
        $this->assertEquals('falsefile', $filter('falsefile'));
    }

    /**
     * @return void
     */
    public function testStringConstructorWithFilesArray()
    {
        $filter = new RenameUploadMock($this->targetFile);
        $this->assertEquals($this->targetFile, $filter->getTarget());
        $this->assertEquals(
            [
                'tmp_name' => $this->targetFile,
                'name'     => $this->targetFile,
            ],
            $filter([
                'tmp_name' => $this->sourceFile,
                'name' => $this->targetFile,
            ])
        );
        $this->assertEquals('falsefile', $filter('falsefile'));
    }

    /**
     * @requires PHP 7
     * @return void
     */
    public function testStringConstructorWithPsrFile()
    {
        $sourceFile = $this->sourceFile;
        $targetFile = $this->targetFile;

        $originalStream = $this->prophesize(StreamInterface::class);
        $originalStream->getMetadata('uri')->willReturn($this->sourceFile);

        $originalFile = $this->prophesize(UploadedFileInterface::class);
        $originalFile->getStream()->will([$originalStream, 'reveal']);
        $originalFile->getClientFilename()->willReturn($targetFile);
        $originalFile
            ->moveTo($targetFile)
            ->will(function ($args) use ($sourceFile) {
                $targetFile = array_shift($args);
                copy($sourceFile, $targetFile);
            })
            ->shouldBeCalled();
        $originalFile->getClientMediaType()->willReturn(null);

        $renamedStream = $this->prophesize(StreamInterface::class);
        $streamFactory = $this->prophesize(StreamFactoryInterface::class);
        $streamFactory
            ->createStreamFromFile($targetFile)
            ->will([$renamedStream, 'reveal']);

        $renamedFile = $this->prophesize(UploadedFileInterface::class);

        $fileFactory = $this->prophesize(UploadedFileFactoryInterface::class);
        $fileFactory
            ->createUploadedFile(
                Argument::that([$renamedStream, 'reveal']),
                0,  // we can hardcode this, as we know the file is empty
                UPLOAD_ERR_OK,
                $targetFile,
                null
            )
            ->will([$renamedFile, 'reveal']);

        $filter = new RenameUploadMock($targetFile);
        $this->assertEquals($targetFile, $filter->getTarget());

        $filter->setStreamFactory($streamFactory->reveal());
        $filter->setUploadFileFactory($fileFactory->reveal());

        $moved = $filter($originalFile->reveal());

        $this->assertSame($renamedFile->reveal(), $moved);
    }

    /**
     * @return void
     */
    public function testArrayConstructorParam()
    {
        $filter = new RenameUploadMock([
            'target' => $this->targetFile,
        ]);
        $this->assertEquals($this->targetFile, $filter->getTarget());
        $this->assertEquals($this->targetFile, $filter($this->sourceFile));
        $this->assertEquals('falsefile', $filter('falsefile'));
    }

    /**
     * @return void
     */
    public function testConstructTruncatedTarget()
    {
        $filter = new FileRenameUpload('*');
        $this->assertEquals('*', $filter->getTarget());
        $this->assertEquals($this->sourceFile, $filter($this->sourceFile));
        $this->assertEquals('falsefile', $filter('falsefile'));
    }

    /**
     * @return void
     */
    public function testTargetDirectory()
    {
        $filter = new RenameUploadMock($this->targetPath);
        $this->assertEquals($this->targetPath, $filter->getTarget());
        $this->assertEquals($this->targetPathFile, $filter($this->sourceFile));
        $this->assertEquals('falsefile', $filter('falsefile'));
    }

    /**
     * @return void
     */
    public function testOverwriteWithExistingFile()
    {
        $filter = new RenameUploadMock([
            'target'          => $this->targetFile,
            'overwrite'       => true,
        ]);

        copy($this->sourceFile, $this->targetFile);

        $this->assertEquals($this->targetFile, $filter->getTarget());
        $this->assertEquals($this->targetFile, $filter($this->sourceFile));
    }

    /**
     * @return void
     */
    public function testCannotOverwriteExistingFile()
    {
        $filter = new RenameUploadMock([
            'target'          => $this->targetFile,
            'overwrite'       => false,
        ]);

        copy($this->sourceFile, $this->targetFile);

        $this->assertEquals($this->targetFile, $filter->getTarget());
        $this->assertFalse($filter->getOverwrite());
        $this->expectException(Exception\InvalidArgumentException::class);
        $this->expectExceptionMessage('already exists');
        $this->assertEquals($this->targetFile, $filter($this->sourceFile));
    }

    /**
     * @return void
     */
    public function testGetRandomizedFile()
    {
        $fileNoExt = $this->filesPath . DIRECTORY_SEPARATOR . 'newfile';
        $filter = new RenameUploadMock([
            'target'          => $this->targetFile,
            'randomize'       => true,
        ]);

        $this->assertRegExp('#' . str_replace('\\', '\\\\', $fileNoExt) . '_.{23}\.xml#', $filter($this->sourceFile));
    }

    public function testGetFileWithOriginalExtension()
    {
        $fileNoExt = $this->filesPath . DIRECTORY_SEPARATOR . 'newfile';
        $filter = new RenameUploadMock([
            'target'          => $this->targetFile,
            'use_upload_extension' => true,
            'randomize'       => false,
        ]);

        $oldFilePathInfo = pathinfo($this->sourceFile);

        $this->assertRegExp(
            '#' . str_replace('\\', '\\\\', $fileNoExt) . '.'.$oldFilePathInfo['extension'].'#',
            $filter($this->sourceFile)
        );
    }

    public function testGetRandomizedFileWithOriginalExtension()
    {
        $fileNoExt = $this->filesPath . DIRECTORY_SEPARATOR . 'newfile';
        $filter = new RenameUploadMock([
            'target'          => $this->targetFile,
            'use_upload_extension' => true,
            'randomize'       => true,
        ]);

        $oldFilePathInfo = pathinfo($this->sourceFile);

        $this->assertRegExp(
            '#' . str_replace('\\', '\\\\', $fileNoExt) . '_.{23}\.'.$oldFilePathInfo['extension'].'#',
            $filter($this->sourceFile)
        );
    }

    /**
     * @return void
     */
    public function testGetRandomizedFileWithoutExtension()
    {
        $fileNoExt = $this->filesPath . DIRECTORY_SEPARATOR . 'newfile';
        $filter = new RenameUploadMock([
            'target'          => $fileNoExt,
            'randomize'       => true,
        ]);

        $this->assertRegExp('#' . str_replace('\\', '\\\\', $fileNoExt) . '_.{13}#', $filter($this->sourceFile));
    }

    /**
     * @return void
     */
    public function testInvalidConstruction()
    {
        $this->expectException(Exception\InvalidArgumentException::class);
        $this->expectExceptionMessage('Invalid target');
        $filter = new FileRenameUpload(1234);
    }

    /**
     * @return void
     */
    public function testCanFilterMultipleTimesWithSameResult()
    {
        $filter = new RenameUploadMock([
            'target'          => $this->targetFile,
            'randomize'       => true,
        ]);

        $firstResult = $filter($this->sourceFile);

        $this->assertContains('newfile', $firstResult);

        $secondResult = $filter($this->sourceFile);

        $this->assertSame($firstResult, $secondResult);
    }


    public function returnUnfilteredDataProvider()
    {
        return [
            [null],
            [new \stdClass()],
            [[
                $this->sourceFile,
                'something invalid'
            ]]
        ];
    }

    /**
     * @dataProvider returnUnfilteredDataProvider
     * @return void
     */
    public function testReturnUnfiltered($input)
    {
        $filter = new RenameUploadMock([
            'target'          => $this->targetFile,
            'randomize'       => true,
        ]);

        $this->assertEquals($input, $filter($input));
    }

    /**
     * @see https://github.com/zendframework/zend-filter/issues/77
     * @return void
     */
    public function testFilterDoesNotAlterUnknownFileDataAndCachesResultsOfFilteringSAPIUploads()
    {
        $filter = new RenameUploadMock($this->targetPath);

        // Emulate the output of \Zend\Http\Request::getFiles()->toArray()
        $sapiSource = [
            'tmp_name' => $this->sourceFile,
            'name' => basename($this->targetFile),
            'type' => 'text/plain',
            'error' => \UPLOAD_ERR_OK,
            'size' => 123,
        ];

        $sapiTarget = [
            'tmp_name' => $this->targetPathFile,
            'name' => basename($this->targetFile),
            'type' => 'text/plain',
            'error' => \UPLOAD_ERR_OK,
            'size' => 123,
        ];

        // Check the result twice for the `alreadyFiltered` cache path
        $this->assertEquals($sapiTarget, $filter($sapiSource));
        $this->assertEquals($sapiTarget, $filter($sapiSource));
    }

    /**
     * @see https://github.com/zendframework/zend-filter/issues/76
     * @return void
     */
    public function testFilterReturnsFileDataVerbatimUnderSAPIWhenTargetPathIsUnspecified()
    {
        $filter = new RenameUploadMock();

        $source = [
            'tmp_name' => $this->sourceFile,
            'name' => basename($this->targetFile),
        ];

        $this->assertEquals($source, $filter($source));
    }
}
