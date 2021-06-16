<?php
/**
 * @see       https://github.com/zendframework/zend-form for the canonical source repository
 * @copyright Copyright (c) 2005-2018 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-form/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\Form\Element;

use ArrayObject;
use PHPUnit\Framework\TestCase;
use stdClass;
use Zend\Form\Element;
use Zend\Form\Element\Collection;
use Zend\Form\Fieldset;
use Zend\Form\Form;
use Zend\Hydrator\ArraySerializable;
use Zend\Hydrator\ArraySerializableHydrator;
use Zend\Hydrator\ClassMethods;
use Zend\Hydrator\ClassMethodsHydrator;
use Zend\Hydrator\ObjectProperty;
use Zend\Hydrator\ObjectPropertyHydrator;
use Zend\InputFilter\ArrayInput;
use ZendTest\Form\TestAsset\AddressFieldset;
use ZendTest\Form\TestAsset\ArrayModel;
use ZendTest\Form\TestAsset\CustomCollection;
use ZendTest\Form\TestAsset\Entity\Address;
use ZendTest\Form\TestAsset\Entity\Phone;
use ZendTest\Form\TestAsset\Entity\Product;
use ZendTest\Form\TestAsset\ProductFieldset;

class CollectionTest extends TestCase
{
    protected $form;
    protected $productFieldset;

    public function setUp()
    {
        $this->form = new \ZendTest\Form\TestAsset\FormCollection();
        $this->productFieldset = new \ZendTest\Form\TestAsset\ProductFieldset();

        parent::setUp();
    }

    public function testCanRetrieveDefaultPlaceholder()
    {
        $placeholder = $this->form->get('colors')->getTemplatePlaceholder();
        $this->assertEquals('__index__', $placeholder);
    }

    public function testCannotAllowNewElementsIfAllowAddIsFalse()
    {
        $collection = $this->form->get('colors');

        $this->assertTrue($collection->allowAdd());
        $collection->setAllowAdd(false);
        $this->assertFalse($collection->allowAdd());

        // By default, $collection contains 2 elements
        $data = [];
        $data[] = 'blue';
        $data[] = 'green';

        $collection->populateValues($data);
        $this->assertEquals(2, count($collection->getElements()));

        $this->expectException('Zend\Form\Exception\DomainException');
        $data[] = 'orange';
        $collection->populateValues($data);
    }

    public function testCanAddNewElementsIfAllowAddIsTrue()
    {
        $collection = $this->form->get('colors');
        $collection->setAllowAdd(true);
        $this->assertTrue($collection->allowAdd());

        // By default, $collection contains 2 elements
        $data = [];
        $data[] = 'blue';
        $data[] = 'green';

        $collection->populateValues($data);
        $this->assertEquals(2, count($collection->getElements()));

        $data[] = 'orange';
        $collection->populateValues($data);
        $this->assertEquals(3, count($collection->getElements()));
    }

    public function testCanRemoveElementsIfAllowRemoveIsTrue()
    {
        $collection = $this->form->get('colors');
        $collection->setAllowRemove(true);
        $this->assertTrue($collection->allowRemove());

        $data = [];
        $data[] = 'blue';
        $data[] = 'green';

        $collection->populateValues($data);
        $this->assertEquals(2, count($collection->getElements()));

        unset($data[0]);

        $collection->populateValues($data);
        $this->assertEquals(1, count($collection->getElements()));
    }

    public function testCanReplaceElementsIfAllowAddAndAllowRemoveIsTrue()
    {
        $collection = $this->form->get('colors');
        $collection->setAllowAdd(true);
        $collection->setAllowRemove(true);

        $data = [];
        $data[] = 'blue';
        $data[] = 'green';

        $collection->populateValues($data);
        $this->assertEquals(2, count($collection->getElements()));

        unset($data[0]);
        $data[] = 'orange';

        $collection->populateValues($data);
        $this->assertEquals(2, count($collection->getElements()));
    }

    public function testCanValidateFormWithCollectionWithoutTemplate()
    {
        $this->form->setData([
            'colors' => [
                '#ffffff',
                '#ffffff'
            ],
            'fieldsets' => [
                [
                    'field' => 'oneValue',
                    'nested_fieldset' => [
                        'anotherField' => 'anotherValue'
                    ]
                ],
                [
                    'field' => 'twoValue',
                    'nested_fieldset' => [
                        'anotherField' => 'anotherValue'
                    ]
                ]
            ]
        ]);

        $this->assertEquals(true, $this->form->isValid());
    }

    public function testCannotValidateFormWithCollectionWithBadColor()
    {
        $this->form->setData([
            'colors' => [
                '#ffffff',
                '123465'
            ],
            'fieldsets' => [
                [
                    'field' => 'oneValue',
                    'nested_fieldset' => [
                        'anotherField' => 'anotherValue'
                    ]
                ],
                [
                    'field' => 'twoValue',
                    'nested_fieldset' => [
                        'anotherField' => 'anotherValue'
                    ]
                ]
            ]
        ]);

        $this->assertEquals(false, $this->form->isValid());
        $messages = $this->form->getMessages();
        $this->assertArrayHasKey('colors', $messages);
    }

    public function testCannotValidateFormWithCollectionWithBadFieldsetField()
    {
        $this->form->setData([
            'colors' => [
                '#ffffff',
                '#ffffff'
            ],
            'fieldsets' => [
                [
                    'field' => 'oneValue',
                    'nested_fieldset' => [
                        'anotherField' => 'anotherValue'
                    ]
                ],
                [
                    'field' => 'twoValue',
                    'nested_fieldset' => [
                        'anotherField' => null,
                    ]
                ]
            ]
        ]);

        $this->assertEquals(false, $this->form->isValid());
        $messages = $this->form->getMessages();
        $this->assertCount(1, $messages);
        $this->assertArrayHasKey('fieldsets', $messages);
    }

    public function testCanValidateFormWithCollectionWithTemplate()
    {
        $collection = $this->form->get('colors');

        $this->assertFalse($collection->shouldCreateTemplate());
        $collection->setShouldCreateTemplate(true);
        $this->assertTrue($collection->shouldCreateTemplate());

        $collection->setTemplatePlaceholder('__template__');

        $this->form->setData([
            'colors' => [
                '#ffffff',
                '#ffffff'
            ],
            'fieldsets' => [
                [
                    'field' => 'oneValue',
                    'nested_fieldset' => [
                        'anotherField' => 'anotherValue'
                    ]
                ],
                [
                    'field' => 'twoValue',
                    'nested_fieldset' => [
                        'anotherField' => 'anotherValue'
                    ]
                ]
            ]
        ]);

        $this->assertEquals(true, $this->form->isValid());
    }

    public function testThrowExceptionIfThereAreLessElementsAndAllowRemoveNotAllowed()
    {
        $this->expectException('Zend\Form\Exception\DomainException');

        $collection = $this->form->get('colors');
        $collection->setAllowRemove(false);

        $this->form->setData([
            'colors' => [
                '#ffffff'
            ],
            'fieldsets' => [
                [
                    'field' => 'oneValue',
                    'nested_fieldset' => [
                        'anotherField' => 'anotherValue'
                    ]
                ],
                [
                    'field' => 'twoValue',
                    'nested_fieldset' => [
                        'anotherField' => 'anotherValue'
                    ]
                ]
            ]
        ]);

        $this->form->isValid();
    }

    public function testCanValidateLessThanSpecifiedCount()
    {
        $collection = $this->form->get('colors');
        $collection->setAllowRemove(true);

        $this->form->setData([
            'colors' => [
                '#ffffff'
            ],
            'fieldsets' => [
                [
                    'field' => 'oneValue',
                    'nested_fieldset' => [
                        'anotherField' => 'anotherValue'
                    ]
                ],
                [
                    'field' => 'twoValue',
                    'nested_fieldset' => [
                        'anotherField' => 'anotherValue'
                    ]
                ]
            ]
        ]);

        $this->assertEquals(true, $this->form->isValid());
    }

    public function testSetOptions()
    {
        $collection = $this->form->get('colors');
        $element = new Element('foo');
        $collection->setOptions([
            'target_element' => $element,
            'count' => 2,
            'allow_add' => true,
            'allow_remove' => false,
            'should_create_template' => true,
            'template_placeholder' => 'foo',
        ]);
        $this->assertInstanceOf('Zend\Form\Element', $collection->getOption('target_element'));
        $this->assertEquals(2, $collection->getOption('count'));
        $this->assertEquals(true, $collection->getOption('allow_add'));
        $this->assertEquals(false, $collection->getOption('allow_remove'));
        $this->assertEquals(true, $collection->getOption('should_create_template'));
        $this->assertEquals('foo', $collection->getOption('template_placeholder'));
    }

    public function testSetObjectNullRaisesException()
    {
        $collection = $this->form->get('colors');
        $this->expectException('Zend\Form\Exception\InvalidArgumentException');
        $collection->setObject(null);
    }

    public function testPopulateValuesNullRaisesException()
    {
        $collection = $this->form->get('colors');
        $this->expectException('Zend\Form\Exception\InvalidArgumentException');
        $collection->populateValues(null);
    }

    public function testSetTargetElementNullRaisesException()
    {
        $collection = $this->form->get('colors');
        $this->expectException('Zend\Form\Exception\InvalidArgumentException');
        $collection->setTargetElement(null);
    }

    public function testGetTargetElement()
    {
        $collection = $this->form->get('colors');
        $element = new Element('foo');
        $collection->setTargetElement($element);

        $this->assertInstanceOf('Zend\Form\Element', $collection->getTargetElement());
    }

    public function testExtractFromObjectDoesntTouchOriginalObject()
    {
        $form = new \Zend\Form\Form();
        $form->setHydrator(
            class_exists(ClassMethodsHydrator::class)
            ? new ClassMethodsHydrator()
            : new ClassMethods()
        );
        $this->productFieldset->setUseAsBaseFieldset(true);
        $form->add($this->productFieldset);

        $originalObjectHash = spl_object_hash(
            $this->productFieldset->get("categories")->getTargetElement()->getObject()
        );

        $product = new Product();
        $product->setName("foo");
        $product->setPrice(42);
        $cat1 = new \ZendTest\Form\TestAsset\Entity\Category();
        $cat1->setName("bar");
        $cat2 = new \ZendTest\Form\TestAsset\Entity\Category();
        $cat2->setName("bar2");

        $product->setCategories([$cat1, $cat2]);

        $form->bind($product);

        $form->setData(
            ["product" =>
                [
                    "name" => "franz",
                    "price" => 13,
                    "categories" => [
                        ["name" => "sepp"],
                        ["name" => "herbert"]
                    ]
                ]
            ]
        );

        $objectAfterExtractHash = spl_object_hash(
            $this->productFieldset->get("categories")->getTargetElement()->getObject()
        );

        $this->assertSame($originalObjectHash, $objectAfterExtractHash);
    }

    public function testDoesNotCreateNewObjects()
    {
        if (! extension_loaded('intl')) {
            // Required by \Zend\I18n\Validator\IsFloat
            $this->markTestSkipped('ext/intl not enabled');
        }

        $form = new \Zend\Form\Form();
        $form->setHydrator(
            class_exists(ClassMethodsHydrator::class)
            ? new ClassMethodsHydrator()
            : new ClassMethods()
        );
        $this->productFieldset->setUseAsBaseFieldset(true);
        $form->add($this->productFieldset);

        $product = new Product();
        $product->setName("foo");
        $product->setPrice(42);
        $cat1 = new \ZendTest\Form\TestAsset\Entity\Category();
        $cat1->setName("bar");
        $cat2 = new \ZendTest\Form\TestAsset\Entity\Category();
        $cat2->setName("bar2");

        $product->setCategories([$cat1, $cat2]);

        $form->bind($product);

        $form->setData(
            ["product" =>
                [
                    "name" => "franz",
                    "price" => 13,
                    "categories" => [
                        ["name" => "sepp"],
                        ["name" => "herbert"]
                    ]
                ]
            ]
        );
        $form->isValid();

        $categories = $product->getCategories();
        $this->assertSame($categories[0], $cat1);
        $this->assertSame($categories[1], $cat2);
    }

    public function testCreatesNewObjectsIfSpecified()
    {
        if (! extension_loaded('intl')) {
            // Required by \Zend\I18n\Validator\IsFloat
            $this->markTestSkipped('ext/intl not enabled');
        }

        $this->productFieldset->setUseAsBaseFieldset(true);
        $categories = $this->productFieldset->get('categories');
        $categories->setOptions([
            'create_new_objects' => true,
        ]);

        $form = new \Zend\Form\Form();
        $form->setHydrator(
            class_exists(ClassMethodsHydrator::class)
            ? new ClassMethodsHydrator()
            : new ClassMethods()
        );
        $form->add($this->productFieldset);

        $product = new Product();
        $product->setName("foo");
        $product->setPrice(42);
        $cat1 = new \ZendTest\Form\TestAsset\Entity\Category();
        $cat1->setName("bar");
        $cat2 = new \ZendTest\Form\TestAsset\Entity\Category();
        $cat2->setName("bar2");

        $product->setCategories([$cat1, $cat2]);

        $form->bind($product);

        $form->setData(
            ["product" =>
                [
                    "name" => "franz",
                    "price" => 13,
                    "categories" => [
                        ["name" => "sepp"],
                        ["name" => "herbert"]
                    ]
                ]
            ]
        );
        $form->isValid();

        $categories = $product->getCategories();
        $this->assertNotSame($categories[0], $cat1);
        $this->assertNotSame($categories[1], $cat2);
    }

    /**
     * @group 6585
     * @group 6614
     */
    public function testAddingCollectionElementAfterBind()
    {
        $form = new Form();
        $form->setHydrator(
            class_exists(ObjectPropertyHydrator::class)
            ? new ObjectPropertyHydrator()
            : new ObjectProperty()
        );

        $phone = new \ZendTest\Form\TestAsset\PhoneFieldset();

        $form->add([
            'name' => 'phones',
            'type' => 'Collection',
            'options' => [
                'target_element' => $phone,
                'count' => 1,
                'allow_add' => true
            ],
        ]);

        $data = [
            'phones' => [
                ['number' => '1234567'],
                ['number' => '1234568'],
            ]
        ];

        $phone = new Phone();
        $phone->setNumber($data['phones'][0]['number']);

        $customer = new stdClass();
        $customer->phones = [$phone];

        $form->bind($customer);
        $form->setData($data);
        $this->assertTrue($form->isValid());
    }

    /**
     * @group 6585
     * @group 6614
     */
    public function testDoesNotCreateNewObjectsWhenUsingNestedCollections()
    {
        $addressesFieldset = new AddressFieldset();
        $addressesFieldset->setHydrator(
            class_exists(ClassMethodsHydrator::class)
            ? new ClassMethodsHydrator()
            : new ClassMethods()
        );
        $addressesFieldset->remove('city');

        $form = new Form();
        $form->setHydrator(
            class_exists(ObjectPropertyHydrator::class)
            ? new ObjectPropertyHydrator()
            : new ObjectProperty()
        );
        $form->add([
            'name' => 'addresses',
            'type' => 'Collection',
            'options' => [
                'target_element' => $addressesFieldset,
                'count' => 1
            ],
        ]);

        $data = [
            'addresses' =>
                [[
                    'street' => 'street1',
                    'phones' =>
                        [['number' => '1234567']],
                ]]
        ];

        $phone  = new Phone();
        $phone->setNumber($data['addresses'][0]['phones'][0]['number']);

        $address = new Address();
        $address->setStreet($data['addresses'][0]['street']);
        $address->setPhones([$phone]);

        $customer = new stdClass();
        $customer->addresses = [$address];

        $form->bind($customer);
        $form->setData($data);

        $this->assertTrue($form->isValid());
        $phones = $customer->addresses[0]->getPhones();
        $this->assertSame($phone, $phones[0]);
    }

    public function testDoNotCreateExtraFieldsetOnMultipleBind()
    {
        $form = new \Zend\Form\Form();
        $this->productFieldset->setHydrator(
            class_exists(ClassMethodsHydrator::class)
            ? new ClassMethodsHydrator()
            : new ClassMethods()
        );
        $form->add($this->productFieldset);
        $form->setHydrator(
            class_exists(ObjectPropertyHydrator::class)
            ? new ObjectPropertyHydrator()
            : new ObjectProperty()
        );

        $product = new Product();
        $categories = [
            new \ZendTest\Form\TestAsset\Entity\Category(),
            new \ZendTest\Form\TestAsset\Entity\Category(),
        ];
        $product->setCategories($categories);

        $market = new \StdClass();
        $market->product = $product;

        // this will pass the test
        $form->bind($market);
        $this->assertSame(count($categories), iterator_count($form->get('product')->get('categories')->getIterator()));

        // this won't pass, but must
        $form->bind($market);
        $this->assertSame(count($categories), iterator_count($form->get('product')->get('categories')->getIterator()));
    }

    public function testExtractDefaultIsEmptyArray()
    {
        $collection = $this->form->get('fieldsets');
        $this->assertEquals([], $collection->extract());
    }

    public function testExtractThroughTargetElementHydrator()
    {
        $collection = $this->form->get('fieldsets');
        $this->prepareForExtract($collection);

        $expected = [
            'obj2' => ['field' => 'fieldOne'],
            'obj3' => ['field' => 'fieldTwo'],
        ];

        $this->assertEquals($expected, $collection->extract());
    }

    public function testExtractMaintainsTargetElementObject()
    {
        $collection = $this->form->get('fieldsets');
        $this->prepareForExtract($collection);

        $expected = $collection->getTargetElement()->getObject();

        $collection->extract();

        $test = $collection->getTargetElement()->getObject();

        $this->assertSame($expected, $test);
    }

    public function testExtractThroughCustomHydrator()
    {
        $collection = $this->form->get('fieldsets');
        $this->prepareForExtract($collection);

        $mockHydrator = $this->createMock('Zend\Hydrator\HydratorInterface');
        $mockHydrator->expects($this->exactly(2))
            ->method('extract')
            ->will($this->returnCallback(function ($object) {
                return ['value' => $object->field . '_foo'];
            }));

        $collection->setHydrator($mockHydrator);

        $expected = [
            'obj2' => ['value' => 'fieldOne_foo'],
            'obj3' => ['value' => 'fieldTwo_foo'],
        ];

        $this->assertEquals($expected, $collection->extract());
    }

    public function testExtractFromTraversable()
    {
        $collection = $this->form->get('fieldsets');
        $this->prepareForExtract($collection);

        $traversable = new ArrayObject($collection->getObject());
        $collection->setObject($traversable);

        $expected = [
            'obj2' => ['field' => 'fieldOne'],
            'obj3' => ['field' => 'fieldTwo'],
        ];

        $this->assertEquals($expected, $collection->extract());
    }

    public function testValidateData()
    {
        $myFieldset = new Fieldset();
        $myFieldset->add([
            'name' => 'email',
            'type' => 'Email',
        ]);

        $myForm = new Form();
        $myForm->add([
            'name' => 'collection',
            'type' => 'Collection',
            'options' => [
                'target_element' => $myFieldset,
            ],
        ]);

        $data = [
            'collection' => [
                ['email' => 'test1@test1.com'],
                ['email' => 'test2@test2.com'],
                ['email' => 'test3@test3.com'],
            ]
        ];

        $myForm->setData($data);

        $this->assertTrue($myForm->isValid());
        $this->assertEmpty($myForm->getMessages());
    }

    protected function prepareForExtract($collection)
    {
        $targetElement = $collection->getTargetElement();

        $obj1 = new stdClass();

        $targetElement
            ->setHydrator(
                class_exists(ObjectPropertyHydrator::class)
                ? new ObjectPropertyHydrator()
                : new ObjectProperty()
            )
            ->setObject($obj1);

        $obj2 = new stdClass();
        $obj2->field = 'fieldOne';

        $obj3 = new stdClass();
        $obj3->field = 'fieldTwo';

        $collection->setObject([
            'obj2' => $obj2,
            'obj3' => $obj3,
        ]);
    }

    public function testCollectionCanBindObjectAndPopulateAndExtractNestedFieldsets()
    {
        $productFieldset = new \ZendTest\Form\TestAsset\ProductFieldset();
        $productFieldset->setHydrator(
            class_exists(ClassMethodsHydrator::class)
            ? new ClassMethodsHydrator()
            : new ClassMethods()
        );

        $mainFieldset = new Fieldset();
        $mainFieldset->setObject(new stdClass);
        $mainFieldset->setHydrator(
            class_exists(ObjectPropertyHydrator::class)
            ? new ObjectPropertyHydrator()
            : new ObjectProperty()
        );
        $mainFieldset->add($productFieldset);

        $form = new Form();
        $form->setHydrator(
            class_exists(ObjectPropertyHydrator::class)
            ? new ObjectPropertyHydrator()
            : new ObjectProperty()
        );
        $form->add([
            'name' => 'collection',
            'type' => 'Collection',
            'options' => [
                'target_element' => $mainFieldset,
                'count' => 2
            ],
        ]);

        $market = new stdClass();

        $prices = [100, 200];
        $categoryNames = ['electronics', 'furniture'];
        $productCountries = ['Russia', 'Jamaica'];

        $shop1 = new stdClass();
        $shop1->product = new Product();
        $shop1->product->setPrice($prices[0]);

        $category = new \ZendTest\Form\TestAsset\Entity\Category();
        $category->setName($categoryNames[0]);
        $shop1->product->setCategories([$category]);

        $country = new  \ZendTest\Form\TestAsset\Entity\Country();
        $country->setName($productCountries[0]);
        $shop1->product->setMadeInCountry($country);



        $shop2 = new stdClass();
        $shop2->product = new Product();
        $shop2->product->setPrice($prices[1]);

        $category = new \ZendTest\Form\TestAsset\Entity\Category();
        $category->setName($categoryNames[1]);
        $shop2->product->setCategories([$category]);

        $country = new  \ZendTest\Form\TestAsset\Entity\Country();
        $country->setName($productCountries[1]);
        $shop2->product->setMadeInCountry($country);



        $market->collection = [$shop1, $shop2];
        $form->bind($market);

        //test for object binding
        $_marketCollection = $form->get('collection');
        $this->assertInstanceOf('Zend\Form\Element\Collection', $_marketCollection);

        foreach ($_marketCollection as $_shopFieldset) {
            $this->assertInstanceOf('Zend\Form\Fieldset', $_shopFieldset);
            $this->assertInstanceOf('stdClass', $_shopFieldset->getObject());

            // test for collection -> fieldset
            $_productFieldset = $_shopFieldset->get('product');
            $this->assertInstanceOf('ZendTest\Form\TestAsset\ProductFieldset', $_productFieldset);
            $this->assertInstanceOf('ZendTest\Form\TestAsset\Entity\Product', $_productFieldset->getObject());

            // test for collection -> fieldset -> fieldset
            $this->assertInstanceOf(
                'ZendTest\Form\TestAsset\CountryFieldset',
                $_productFieldset->get('made_in_country')
            );
            $this->assertInstanceOf(
                'ZendTest\Form\TestAsset\Entity\Country',
                $_productFieldset->get('made_in_country')->getObject()
            );

            // test for collection -> fieldset -> collection
            $_productCategories = $_productFieldset->get('categories');
            $this->assertInstanceOf('Zend\Form\Element\Collection', $_productCategories);

            // test for collection -> fieldset -> collection -> fieldset
            foreach ($_productCategories as $_category) {
                $this->assertInstanceOf('ZendTest\Form\TestAsset\CategoryFieldset', $_category);
                $this->assertInstanceOf('ZendTest\Form\TestAsset\Entity\Category', $_category->getObject());
            }
        };

        // test for correct extract and populate form values
        // test for collection -> fieldset -> field value
        foreach ($prices as $_k => $_price) {
            $this->assertEquals(
                $_price,
                $form->get('collection')->get($_k)
                    ->get('product')
                    ->get('price')
                    ->getValue()
            );
        }

        // test for collection -> fieldset -> fieldset ->field value
        foreach ($productCountries as $_k => $_countryName) {
            $this->assertEquals(
                $_countryName,
                $form->get('collection')->get($_k)
                    ->get('product')
                    ->get('made_in_country')
                    ->get('name')
                    ->getValue()
            );
        }

        // test collection -> fieldset -> collection -> fieldset -> field value
        foreach ($categoryNames as $_k => $_categoryName) {
            $this->assertEquals(
                $_categoryName,
                $form->get('collection')->get($_k)
                    ->get('product')
                    ->get('categories')->get(0)
                    ->get('name')->getValue()
            );
        }
    }

    public function testExtractFromTraversableImplementingToArrayThroughCollectionHydrator()
    {
        $collection = $this->form->get('fieldsets');

        // this test is using a hydrator set on the collection
        $collection->setHydrator(
            class_exists(ArraySerializableHydrator::class)
            ? new ArraySerializableHydrator()
            : new ArraySerializable()
        );

        $this->prepareForExtractWithCustomTraversable($collection);

        $expected = [
            ['foo' => 'foo_value_1', 'bar' => 'bar_value_1', 'foobar' => 'foobar_value_1'],
            ['foo' => 'foo_value_2', 'bar' => 'bar_value_2', 'foobar' => 'foobar_value_2'],
        ];

        $this->assertEquals($expected, $collection->extract());
    }

    public function testExtractFromTraversableImplementingToArrayThroughTargetElementHydrator()
    {
        $collection = $this->form->get('fieldsets');

        // this test is using a hydrator set on the target element of the collection
        $targetElement = $collection->getTargetElement();
        $targetElement->setHydrator(
            class_exists(ArraySerializableHydrator::class)
            ? new ArraySerializableHydrator()
            : new ArraySerializable()
        );
        $obj1 = new ArrayModel();
        $targetElement->setObject($obj1);

        $this->prepareForExtractWithCustomTraversable($collection);

        $expected = [
            ['foo' => 'foo_value_1', 'bar' => 'bar_value_1', 'foobar' => 'foobar_value_1'],
            ['foo' => 'foo_value_2', 'bar' => 'bar_value_2', 'foobar' => 'foobar_value_2'],
        ];

        $this->assertEquals($expected, $collection->extract());
    }

    protected function prepareForExtractWithCustomTraversable($collection)
    {
        $obj2 = new ArrayModel();
        $obj2->exchangeArray(['foo' => 'foo_value_1', 'bar' => 'bar_value_1', 'foobar' => 'foobar_value_1']);
        $obj3 = new ArrayModel();
        $obj3->exchangeArray(['foo' => 'foo_value_2', 'bar' => 'bar_value_2', 'foobar' => 'foobar_value_2']);

        $traversable = new CustomCollection();
        $traversable->append($obj2);
        $traversable->append($obj3);
        $collection->setObject($traversable);
    }

    public function testPopulateValuesWithFirstKeyGreaterThanZero()
    {
        $inputData = [
            1 => ['name' => 'black'],
            5 => ['name' => 'white'],
        ];

        // Standalone Collection element
        $collection = new Collection('fieldsets', [
            'count' => 1,
            'target_element' => new \ZendTest\Form\TestAsset\CategoryFieldset(),
        ]);

        $form = new Form();
        $form->add([
            'type' => 'Zend\Form\Element\Collection',
            'name' => 'collection',
            'options' => [
                'count' => 1,
                'target_element' => new \ZendTest\Form\TestAsset\CategoryFieldset(),
            ]
        ]);

        // Collection element attached to a form
        $formCollection = $form->get('collection');

        $collection->populateValues($inputData);
        $formCollection->populateValues($inputData);

        $this->assertEquals(count($collection->getFieldsets()), count($inputData));
        $this->assertEquals(count($formCollection->getFieldsets()), count($inputData));
    }

    public function testCanRemoveAllElementsIfAllowRemoveIsTrue()
    {
        /**
         * @var \Zend\Form\Element\Collection $collection
         */
        $collection = $this->form->get('colors');
        $collection->setAllowRemove(true);
        $collection->setCount(0);

        // By default, $collection contains 2 elements
        $data = [];
        $data[] = 'blue';
        $data[] = 'green';

        $collection->populateValues($data);
        $this->assertEquals(2, count($collection->getElements()));

        $collection->populateValues([]);
        $this->assertEquals(0, count($collection->getElements()));
    }

    public function testCanBindObjectMultipleNestedFieldsets()
    {
        $productFieldset = new ProductFieldset();
        $productFieldset->setHydrator(
            class_exists(ArraySerializableHydrator::class)
            ? new ArraySerializableHydrator()
            : new ArraySerializable()
        );
        $productFieldset->setObject(new Product());

        $nestedFieldset = new Fieldset('nested');
        $nestedFieldset->setHydrator(
            class_exists(ObjectPropertyHydrator::class)
            ? new ObjectPropertyHydrator()
            : new ObjectProperty()
        );
        $nestedFieldset->setObject(new stdClass());
        $nestedFieldset->add([
            'name' => 'products',
            'type' => 'Collection',
            'options' => [
                'target_element' => $productFieldset,
                'count' => 2,
            ],
        ]);

        $mainFieldset = new Fieldset('main');
        $mainFieldset->setUseAsBaseFieldset(true);
        $mainFieldset->setHydrator(
            class_exists(ObjectPropertyHydrator::class)
            ? new ObjectPropertyHydrator()
            : new ObjectProperty()
        );
        $mainFieldset->setObject(new stdClass());
        $mainFieldset->add([
            'name' => 'nested',
            'type' => 'Collection',
            'options' => [
                'target_element' => $nestedFieldset,
                'count' => 2,
            ],
        ]);

        $form = new Form();
        $form->setHydrator(
            class_exists(ObjectPropertyHydrator::class)
            ? new ObjectPropertyHydrator()
            : new ObjectProperty()
        );
        $form->add($mainFieldset);

        $market = new stdClass();

        $prices = [100, 200];

        $products[0] = new Product();
        $products[0]->setPrice($prices[0]);
        $products[1] = new Product();
        $products[1]->setPrice($prices[1]);

        $shop[0] = new stdClass();
        $shop[0]->products = $products;

        $shop[1] = new stdClass();
        $shop[1]->products = $products;

        $market->nested = $shop;
        $form->bind($market);

        //test for object binding

        // Main fieldset has a collection 'nested'...
        $this->assertCount(1, $form->get('main')->getFieldsets());
        foreach ($form->get('main')->getFieldsets() as $_fieldset) {
            // ...which contains two stdClass objects (shops)
            $this->assertCount(2, $_fieldset->getFieldsets());
            foreach ($_fieldset->getFieldsets() as $_nestedfieldset) {
                // Each shop is represented by a single fieldset
                $this->assertCount(1, $_nestedfieldset->getFieldsets());
                foreach ($_nestedfieldset->getFieldsets() as $_productfieldset) {
                    // Each shop fieldset contain a collection with two products in it
                    $this->assertCount(2, $_productfieldset->getFieldsets());
                    foreach ($_productfieldset->getFieldsets() as $_product) {
                        $this->assertInstanceOf('ZendTest\Form\TestAsset\Entity\Product', $_product->getObject());
                    }
                }
            }
        };
    }

    public function testNestedCollections()
    {
        // @see https://github.com/zendframework/zf2/issues/5640
        $addressesFieldeset = new \ZendTest\Form\TestAsset\AddressFieldset();
        $addressesFieldeset->setHydrator(
            class_exists(ClassMethodsHydrator::class)
            ? new ClassMethodsHydrator()
            : new ClassMethods()
        );

        $form = new Form();
        $form->setHydrator(
            class_exists(ObjectPropertyHydrator::class)
            ? new ObjectPropertyHydrator()
            : new ObjectProperty()
        );
        $form->add([
            'name' => 'addresses',
            'type' => 'Collection',
            'options' => [
                'target_element' => $addressesFieldeset,
                'count' => 2
            ],
        ]);

        $data = [
            ['number' => '0000000001', 'street' => 'street1'],
            ['number' => '0000000002', 'street' => 'street2'],
        ];

        $phone1 = new Phone();
        $phone1->setNumber($data[0]['number']);

        $phone2 = new Phone();
        $phone2->setNumber($data[1]['number']);

        $address1 = new Address();
        $address1->setStreet($data[0]['street']);
        $address1->setPhones([$phone1]);

        $address2 = new Address();
        $address2->setStreet($data[1]['street']);
        $address2->setPhones([$phone2]);

        $customer = new stdClass();
        $customer->addresses = [$address1, $address2];

        $form->bind($customer);

        //test for object binding
        foreach ($form->get('addresses')->getFieldsets() as $_fieldset) {
            $this->assertInstanceOf('ZendTest\Form\TestAsset\Entity\Address', $_fieldset->getObject());
            foreach ($_fieldset->getFieldsets() as $_childFieldsetName => $_childFieldset) {
                switch ($_childFieldsetName) {
                    case 'city':
                        $this->assertInstanceOf('ZendTest\Form\TestAsset\Entity\City', $_childFieldset->getObject());
                        break;
                    case 'phones':
                        foreach ($_childFieldset->getFieldsets() as $_phoneFieldset) {
                            $this->assertInstanceOf(
                                'ZendTest\Form\TestAsset\Entity\Phone',
                                $_phoneFieldset->getObject()
                            );
                        }
                        break;
                }
            }
        }

        //test for correct extract and populate
        $index = 0;
        foreach ($form->get('addresses') as $_addresses) {
            $this->assertEquals($data[$index]['street'], $_addresses->get('street')->getValue());
            //assuming data has just 1 phone entry
            foreach ($_addresses->get('phones') as $phone) {
                $this->assertEquals($data[$index]['number'], $phone->get('number')->getValue());
            }
            $index++;
        }
    }

    public function testSetDataOnFormPopulatesCollection()
    {
        $form = new Form();
        $form->add([
            'name' => 'names',
            'type' => 'Collection',
            'options' => [
                'target_element' => new Element\Text(),
            ],
        ]);

        $names = ['foo', 'bar', 'baz', 'bat'];

        $form->setData([
            'names' => $names
        ]);

        $this->assertCount(count($names), $form->get('names'));

        $i = 0;
        foreach ($form->get('names') as $field) {
            $this->assertEquals($names[$i], $field->getValue());
            $i++;
        };
    }

    public function testSettingSomeDataButNoneForCollectionReturnsSpecifiedNumberOfElementsAfterPrepare()
    {
        $form = new Form();
        $form->add(new Element\Text('input'));
        $form->add([
            'name' => 'names',
            'type' => 'Collection',
            'options' => [
                'target_element' => new Element\Text(),
                'count' => 2
            ],
        ]);

        $form->setData([
            'input' => 'foo',
        ]);

        $this->assertCount(0, $form->get('names'));

        $form->prepare();

        $this->assertCount(2, $form->get('names'));
    }

    public function testMininumLenghtIsMaintanedWhenSettingASmallerCollection()
    {
        $arrayCollection = [
            new Element\Color(),
            new Element\Color(),
        ];

        $collection = $this->form->get('colors');
        $collection->setCount(3);
        $collection->setObject($arrayCollection);
        $this->assertEquals(3, $collection->getCount());
    }

    /**
     * @group zf6263
     * @group zf6518
     */
    public function testCollectionProperlyHandlesAddingObjectsOfTypeElementInterface()
    {
        $form = new Form('test');
        $text = new Element\Text('text');
        $form->add([
            'name' => 'text',
            'type' => 'Zend\Form\Element\Collection',
            'options' => [
                'target_element' => $text, 'count' => 2,
            ],
        ]);
        $object = new \ArrayObject(['text' => ['Foo', 'Bar']]);
        $form->bind($object);
        $this->assertTrue($form->isValid());

        $result = $form->getData();
        $this->assertInstanceOf('ArrayAccess', $result);
        $this->assertArrayHasKey('text', $result);
        $this->assertInternalType('array', $result['text']);
        $this->assertArrayHasKey(0, $result['text']);
        $this->assertEquals('Foo', $result['text'][0]);
        $this->assertArrayHasKey(1, $result['text']);
        $this->assertEquals('Bar', $result['text'][1]);
    }

    /**
     * Unit test to ensure behavior of extract() method is unaffected by refactor
     *
     * @group zf6263
     * @group zf6518
     */
    public function testCollectionShouldSilentlyIgnorePopulatingFieldsetWithDisallowedObject()
    {
        $mainFieldset = new Fieldset();
        $mainFieldset->add(new Element\Text('test'));
        $mainFieldset->setObject(new \ArrayObject());
        $mainFieldset->setHydrator(
            class_exists(ObjectPropertyHydrator::class)
            ? new ObjectPropertyHydrator()
            : new ObjectProperty()
        );

        $form = new Form();
        $form->setObject(new \stdClass());
        $form->setHydrator(
            class_exists(ObjectPropertyHydrator::class)
            ? new ObjectPropertyHydrator()
            : new ObjectProperty()
        );
        $form->add([
            'name' => 'collection',
            'type' => 'Collection',
            'options' => [
                'target_element' => $mainFieldset,
                'count' => 2
            ],
        ]);

        $model = new \stdClass();
        $model->collection = [new \ArrayObject(['test' => 'bar']), new \stdClass()];

        $form->bind($model);
        $this->assertTrue($form->isValid());

        $result = $form->getData();
        $this->assertInstanceOf('stdClass', $result);
        $this->assertObjectHasAttribute('collection', $result);
        $this->assertInternalType('array', $result->collection);
        $this->assertCount(1, $result->collection);
        $this->assertInstanceOf('ArrayObject', $result->collection[0]);
        $this->assertArrayHasKey('test', $result->collection[0]);
        $this->assertEquals('bar', $result->collection[0]['test']);
    }

    /**
     * @group 6263
     * @group 6298
     */
    public function testCanHydrateObject()
    {
        $form = $this->form;
        $object = new \ArrayObject();
        $form->bind($object);
        $data = [
            'colors' => [
                '#ffffff',
            ],
        ];
        $form->setData($data);
        $this->assertTrue($form->isValid());
        $this->assertInternalType('array', $object['colors']);
        $this->assertCount(1, $object['colors']);
    }

    public function testCanRemoveMultipleElements()
    {
        /**
         * @var \Zend\Form\Element\Collection $collection
         */
        $collection = $this->form->get('colors');
        $collection->setAllowRemove(true);
        $collection->setCount(0);

        $data = [];
        $data[] = 'blue';
        $data[] = 'green';
        $data[] = 'red';

        $collection->populateValues($data);

        $collection->populateValues(['colors' => ['0' => 'blue']]);
        $this->assertEquals(1, count($collection->getElements()));
    }

    public function testGetErrorMessagesForInvalidCollectionElements()
    {
        // Configure InputFilter
        $inputFilter = $this->form->getInputFilter();
        $inputFilter->add(
            [
                'name'     => 'colors',
                'type'     => ArrayInput::class,
                'required' => true,
            ]
        );
        $inputFilter->add(
            [
                'name'     => 'fieldsets',
                'type'     => ArrayInput::class,
                'required' => true,
            ]
        );


        $this->form->setData([]);
        $this->form->isValid();

        $this->assertEquals(
            [
                'colors'     => [
                    'isEmpty' => "Value is required and can't be empty",
                ],
                'fieldsets' => [
                    'isEmpty' => "Value is required and can't be empty",
                ],
            ],
            $this->form->getMessages()
        );
    }
}
