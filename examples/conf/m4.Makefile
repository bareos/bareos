ETCDIR=/opt/bacula/etc
M4=/usr/ccs/bin/m4
DIR=/opt/bacula/sbin/bacula-dir
FD=/opt/bacula/sbin/bacula-fd
SD=/opt/bacula/sbin/bacula-sd
BCON=/opt/bacula/sbin/bconsole

all: 	$(ETCDIR)/bacula-dir.conf $(ETCDIR)/bacula-sd.conf \
	$(ETCDIR)/bacula-fd.conf $(ETCDIR)/bconsole.conf 

$(ETCDIR)/bacula-dir.conf: bacula-dir.conf bacula-defs.m4
	$(M4) bacula-dir.conf >$(ETCDIR)/bacula-dir.tmp && \
	$(DIR) -t -c $(ETCDIR)/bacula-dir.tmp && \
	mv $(ETCDIR)/bacula-dir.tmp $(ETCDIR)/bacula-dir.conf

$(ETCDIR)/bacula-sd.conf: bacula-sd.conf bacula-defs.m4
	$(M4) bacula-sd.conf >$(ETCDIR)/bacula-sd.tmp && \
	$(SD) -t -c $(ETCDIR)/bacula-sd.tmp && \
	mv $(ETCDIR)/bacula-sd.tmp $(ETCDIR)/bacula-sd.conf

$(ETCDIR)/bacula-fd.conf: bacula-fd.conf bacula-defs.m4
	$(M4) bacula-fd.conf >$(ETCDIR)/bacula-fd.tmp && \
	$(FD) -t -c $(ETCDIR)/bacula-fd.tmp && \
	mv $(ETCDIR)/bacula-fd.tmp $(ETCDIR)/bacula-fd.conf

$(ETCDIR)/bconsole.conf: bconsole.conf bacula-defs.m4
	$(M4) bconsole.conf >$(ETCDIR)/bconsole.tmp && \
	$(BCON) -t -c $(ETCDIR)/bconsole.tmp && \
	mv $(ETCDIR)/bconsole.tmp $(ETCDIR)/bconsole.conf
