# Quick Start

## Usage in a zend-mvc-based application

The fastest way to get up and running with zend-navigation is:

- Register zend-navigation as module.
- Define navigation container configuration under the top-level `navigation` key
  in your application configuration.
- Render your container using a navigation view helper within your view scripts.

### Register zend-navigation as module

Edit the application configuration file `config/application.config.php`:

```php
<?php
return [
    'modules' => [
        'Zend\Router',
        'Zend\Log',
        'Zend\Navigation', // <-- Add this line
        // ...
    ],
];
```

### Navigation container configuration

Add the container definition to your configuration file, e.g.
`config/autoload/global.php`:

```php
<?php
return [
    // ...

    'navigation' => [
        'default' => [
            [
                'label' => 'Home',
                'route' => 'home',
            ],
            [
                'label' => 'Page #1',
                'route' => 'page-1',
                'pages' => [
                    [
                        'label' => 'Child #1',
                        'route' => 'page-1-child',
                    ],
                ],
            ],
            [
                'label' => 'Page #2',
                'route' => 'page-2',
            ],
        ],
    ],
    // ...
];
```

### Render the navigation

Calling the view helper for menus in your layout script:

```php
<!-- ... -->

<body>
    <?= $this->navigation('default')->menu() ?>
</body>
<!-- ... -->
```

## Using multiple navigations

Once the zend-navigation module is registered, you can create as many navigation
definitions as you wish, and the underlying factories will create navigation
containers automatically.

Add the container definitions to your configuration file, e.g.
`config/autoload/global.php`:

```php
<?php
return [
    // ...

    'navigation' => [

        // Navigation with name default
        'default' => [
            [
                'label' => 'Home',
                'route' => 'home',
            ],
            [
                'label' => 'Page #1',
                'route' => 'page-1',
                'pages' => [
                    [
                        'label' => 'Child #1',
                        'route' => 'page-1-child',
                    ],
                ],
            ],
            [
                'label' => 'Page #2',
                'route' => 'page-2',
            ],
        ],

        // Navigation with name special
        'special' => [
            [
                'label' => 'Special',
                'route' => 'special',
            ],
            [
                'label' => 'Special Page #2',
                'route' => 'special-2',
            ],
        ],

        // Navigation with name sitemap
        'sitemap' => [
            [
                'label' => 'Sitemap',
                'route' => 'sitemap',
            ],
            [
                'label' => 'Sitemap Page #2',
                'route' => 'sitemap-2',
            ],
        ],
    ],
    // ...
];
```

> ### Container names have a prefix
>
> There is one important point to know when using zend-navigation as a module:
> The name of the container in your view script **must** be prefixed with
> `Zend\Navigation\`, followed by the name of the configuration key.
> This helps ensure that no naming collisions occur with other services.

The following example demonstrates rendering the navigation menus for the named
`default`, `special`, and `sitemap` containers.

```php
<!-- ... -->

<body>
    <?= $this->navigation('Zend\Navigation\Default')->menu() ?>

    <?= $this->navigation('Zend\Navigation\Special')->menu() ?>

    <?= $this->navigation('Zend\Navigation\Sitemap')->menu() ?>
</body>
<!-- ... -->
```
