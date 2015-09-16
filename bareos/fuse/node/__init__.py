files = ['backups', 'base', 'bvfs', 'client', 'clients', 'directory', 'file', 'job', 'joblog', 'jobs', 'volume', 'volumes']

for i in files:
    module = __import__( "bareos.fuse.node." + i, globals(), locals(), ['*'])
    for k in dir(module):
        locals()[k] = getattr(module, k)
    del k
del i
del files
