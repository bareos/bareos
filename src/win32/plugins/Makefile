#
# BAREOS® - Backup Archiving REcovery Open Sourced
#
# Copyright (C) 2014-2014 Bareos GmbH & Co. KG
#
# This program is Free Software; you can redistribute it and/or
# modify it under the terms of version three of the GNU Affero General Public
# License as published by the Free Software Foundation and included
# in the file LICENSE.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301, USA.
#
all_subdirs = filed stored dird

all:
	@for I in ${all_subdirs}; \
	  do (cd $$I; echo "==>Entering directory `pwd`"; \
		$(MAKE) WIN_VERSION=$(WIN_VERSION) DESTDIR=$(DESTDIR) $@ || (echo ""; echo ""; echo "  ====== Error in `pwd` ======"; \
		echo ""; echo ""; exit 1;)); \
	done

install:
	@for I in ${all_subdirs}; \
	  do (cd $$I; echo "==>Entering directory `pwd`"; \
		$(MAKE) WIN_VERSION=$(WIN_VERSION) DESTDIR=$(DESTDIR) $@ || (echo ""; echo ""; echo "  ====== Error in `pwd` ======"; \
		echo ""; echo ""; exit 1;)); \
	done

clean:
	@for I in ${all_subdirs}; \
	  do (cd $$I; echo "==>Entering directory `pwd`"; \
		$(MAKE) WIN_VERSION=$(WIN_VERSION) DESTDIR=$(DESTDIR) $@ || (echo ""; echo ""; echo "  ====== Error in `pwd` ======"; \
		echo ""; echo ""; exit 1;)); \
	done

distclean:
	@for I in ${all_subdirs}; \
	  do (cd $$I; echo "==>Entering directory `pwd`"; \
		$(MAKE) WIN_VERSION=$(WIN_VERSION) DESTDIR=$(DESTDIR) $@ || (echo ""; echo ""; echo "  ====== Error in `pwd` ======"; \
		echo ""; echo ""; exit 1;)); \
	done
