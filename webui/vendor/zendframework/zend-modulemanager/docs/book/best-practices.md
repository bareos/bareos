# Best Practices when Creating Modules

When creating a ZF2 module, there are some best practices you should keep in mind.

## Keep the `init()` and `onBootstrap()` methods lightweight

Be conservative with the actions you perform in the `init()` and `onBootstrap()`
methods of your `Module` class. These methods are run for **every** page
request, and should not perform anything heavy. As a rule of thumb, registering
event listeners is an appropriate task to perform in these methods. Such
lightweight tasks will generally not have a measurable impact on the performance
of your application, even with many modules enabled. It is considered bad
practice to utilize these methods for setting up or configuring instances of
application resources such as a database connection, application logger, or
mailer. Tasks such as these are better served through the `ServiceManager`
capabilities.

## Do not perform writes within a module

You should **never** code your module to perform or expect any writes within the
module's directory.  Once installed, the files within a module's directory
should always match the distribution verbatim.  Any user-provided configuration
should be performed via overrides in the `Application` module or via
application-level configuration files. Any other required filesystem writes
should be performed in some writeable path that is outside of the module's
directory.

There are two primary advantages to following this rule. First, any modules
which attempt to write within themselves will not be compatible with phar
packaging. Second, by keeping the module in sync with the upstream distribution,
updates via mechanisms such as git will be simple and trouble-free.  Of course,
the `Application` module is a special exception to this rule, as there is
typically no upstream distribution for this module, and it's unlikely you would
want to run this package from within a phar archive.

## Utilize a vendor prefix for module names

To avoid module naming conflicts, you are encouraged to prefix your module
namespace with a vendor prefix. As an example, the developer tools module
distributed by Zend is named "ZendDeveloperTools" instead of simply
"DeveloperTools".

## Utilize a module prefix for service names

If you define services in the top-level service manager, you are encouraged to
prefix these services with the name of your module to avoid conflicts with other
modules' services. For example, the database adapter used by MyModule should be
called "MyModuleDbAdapter" rather than simply "DbAdapter." If you need to share
a service with other modules, remember that the Service Manager "alias" feature
can be used in a merged configuration to override factories defined by
individual modules. Ideally, modules should define their own service
dependencies, but aliases can be configured at the application level to ensure
that common services in individual modules all refer to the same instance.
