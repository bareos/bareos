## INSTALLATION

As we are in alpha state, there is no installation routine so far.

Please follow these steps to get a working barbossa for development purposes on your system.

### Get the current version of barbossa from github

1. cd into your webservers root directory e.g. /var/www/htdocs

2. Get the current version from github.

```
git clone https://github.com/fbergkemper/barbossa.git
```

3. cd into the barbossa directory

4. Execute composer.phar, which will download all the dependencies (Zend Framework 2, Zend Developers tools) into the vendor directory.

```
./composer.phar self-update
./composer.phar install
```
### Configure Apache

See the example file [barbossa/install/apache/barbossa.conf](https://github.com/fbergkemper/barbossa/blob/master/install/apache/barbossa.conf) .

You might configure your Apache manually or copy the example configuration with wget from the git repo, e.g.

```
cd /etc/apache2/conf.d
wget https://raw2.github.com/fbergkemper/barbossa/master/install/apache/barbossa.conf 
```

### Configure the database connection

1. Open the file barbossa/config/autoload/global.php and edit it to your needs. Currently only PostgreSQL has been tested.

2. Copy barbossa/config/autoload/local.php.dist to barbossa/config/autoload/local.php and edit the local.php file to your needs.

### Configuration to be able to run bconsole from barbossa

For testing and development the easiest way is to add the user under which apache is running to the bareos group, e.g.

```
usermod -aG bareos wwwrun
```

Next, setup bconsole can be executed under Apache webserver.

```
chown root:bareos /usr/sbin/bconsole
chmod 750 /usr/sbin/bconsole

chown root:bareos /etc/bareos/bconsole.conf
chmod 740 /etc/bareos/bconsole.conf
```
