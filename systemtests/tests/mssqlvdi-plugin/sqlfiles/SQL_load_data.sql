--   BAREOS® - Backup Archiving REcovery Open Sourced
--
--   Copyright (C) 2025 Bareos GmbH & Co. KG
--
--   This program is Free Software; you can redistribute it and/or
--   modify it under the terms of version three of the GNU Affero General Public
--   License as published by the Free Software Foundation and included
--   in the file LICENSE.
--
--   This program is distributed in the hope that it will be useful, but
--   WITHOUT ANY WARRANTY; without even the implied warranty of
--   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
--   Affero General Public License for more details.
--
--   You should have received a copy of the GNU Affero General Public License
--   along with this program; if not, write to the Free Software
--   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
--   02110-1301, USA.

USE [$(myDB)]
GO

SET IDENTITY_INSERT tests.samples ON;

INSERT INTO tests.samples(sample_id,sample_name) VALUES(1,'sample 1')
GO
INSERT INTO tests.samples(sample_id,sample_name) VALUES(2,'sample 2')
GO
INSERT INTO tests.samples(sample_id,sample_name) VALUES(3,'sample 3')
GO
INSERT INTO tests.samples(sample_id,sample_name) VALUES(4,'sample 4')
GO
INSERT INTO tests.samples(sample_id,sample_name) VALUES(5,'sample 5')
GO
INSERT INTO tests.samples(sample_id,sample_name) VALUES(6,'sample 6')
GO
INSERT INTO tests.samples(sample_id,sample_name) VALUES(7,'sample 7')
GO
INSERT INTO tests.samples(sample_id,sample_name) VALUES(8,'sample 8')
GO
INSERT INTO tests.samples(sample_id,sample_name) VALUES(9,'sample 9')
GO

SET IDENTITY_INSERT tests.samples OFF;
