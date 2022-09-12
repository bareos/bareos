# Session Manager

The session manager, `Zend\Session\SessionManager`, is the class responsible for
all aspects of session management. It initializes configuration, storage, and
save handlers.  Additionally the session manager can be injected into the
session container to provide a wrapper or namespace around your session data.

The session manager is responsible for starting a session, testing if a session
exists, writing to the session, regenerating the session identifier, setting the
session time-to-live, and destroying the session. The session manager can
validate sessions from a validator chain to ensure that the session data is
correct.

## Initializing the Session Manager

Generally speaking, you will always want to initialize the session manager and
ensure that your application was responsible for its initialization; this puts
in place a simple solution to prevent against session fixation. Generally you
will setup configuration and then inside of an application module bootstrap the
session manager.

Additionally you will likely want to supply validators to prevent against
session hijacking.

The following illustrates how you may configure the session manager by setting
options in your local or global config:

```php
use Zend\Session;

return [
    'session_manager' => [
        'config' => [
            'class' => Session\Config\SessionConfig::class,
            'options' => [
                'name' => 'myapp',
            ],
        ],
        'storage' => Session\Storage\SessionArrayStorage::class,
        'validators' => [
            Session\Validator\RemoteAddr::class,
            Session\Validator\HttpUserAgent::class,
        ],
    ],
];
```

The following illustrates how you might utilize the above configuration to
create the session manager:

```php
use Zend\Session\SessionManager;
use Zend\Session\Config\SessionConfig;
use Zend\Session\Container;
use Zend\Session\SessionManager;
use Zend\Session\Validator;

class Module
{
    public function onBootstrap($e)
    {
        $eventManager        = $e->getApplication()->getEventManager();
        $moduleRouteListener = new ModuleRouteListener();
        $moduleRouteListener->attach($eventManager);
        $this->bootstrapSession($e);
    }

    public function bootstrapSession($e)
    {
        $session = $e->getApplication()
            ->getServiceManager()
            ->get(SessionManager::class);
        $session->start();

        $container = new Container('initialized');

        if (isset($container->init)) {
            return;
        }

        $serviceManager = $e->getApplication()->getServiceManager();
        $request        = $serviceManager->get('Request');

        $session->regenerateId(true);
        $container->init          = 1;
        $container->remoteAddr    = $request->getServer()->get('REMOTE_ADDR');
        $container->httpUserAgent = $request->getServer()->get('HTTP_USER_AGENT');

        $config = $serviceManager->get('Config');
        if (! isset($config['session'])) {
            return;
        }

        $sessionConfig = $config['session'];

        if (! isset($sessionConfig['validators'])) {
            return;
        }

        $chain   = $session->getValidatorChain();

        foreach ($sessionConfig['validators'] as $validator) {
            switch ($validator) {
            case Validator\HttpUserAgent::class:
                    $validator = new $validator($container->httpUserAgent);
                    break;
                    case Validator\RemoteAddr::class:
                    $validator  = new $validator($container->remoteAddr);
                    break;
                default:
                    $validator = new $validator();
            }

            $chain->attach('session.validate', array($validator, 'isValid'));
        }
    }

    public function getServiceConfig()
    {
        return [
            'factories' => [
                SessionManager::class => function ($container) {
                    $config = $container->get('config');
                    if (! isset($config['session'])) {
                        $sessionManager = new SessionManager();
                        Container::setDefaultManager($sessionManager);
                        return $sessionManager;
                    }

                    $session = $config['session'];

                    $sessionConfig = null;
                    if (isset($session['config'])) {
                        $class = isset($session['config']['class'])
                            ?  $session['config']['class']
                            : SessionConfig::class;

                        $options = isset($session['config']['options'])
                            ?  $session['config']['options']
                            : [];

                        $sessionConfig = new $class();
                        $sessionConfig->setOptions($options);
                    }

                    $sessionStorage = null;
                    if (isset($session['storage'])) {
                        $class = $session['storage'];
                        $sessionStorage = new $class();
                    }

                    $sessionSaveHandler = null;
                    if (isset($session['save_handler'])) {
                        // class should be fetched from service manager
                        // since it will require constructor arguments
                        $sessionSaveHandler = $container->get($session['save_handler']);
                    }

                    $sessionManager = new SessionManager(
                        $sessionConfig,
                        $sessionStorage,
                        $sessionSaveHandler
                    );

                    Container::setDefaultManager($sessionManager);
                    return $sessionManager;
                },
            ],
        ];
    }
}
```

When you create a new `Zend\Session\Container` (see
[Session Container](container.md) page) in a controller, for example, it will
use the session configured above.
