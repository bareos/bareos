# ls *.py | sed -r "s/(.*)\.py/'\1', /" | tr '\n' ' '
files = ['backups',  'base',  'bvfsdir',  'bvfsfile',  'client',  'clients',  'directory',  'file',  'job',  'joblog',  'jobs',  'jobslist',  'jobsname',  'pool', 'pools', 'status', 'volume',  'volumelist', 'volumestatus']

for i in files:
    module = __import__( "bareos.fuse.node." + i, globals(), locals(), ['*'])
    for k in dir(module):
        locals()[k] = getattr(module, k)
    del k
del i
del files
