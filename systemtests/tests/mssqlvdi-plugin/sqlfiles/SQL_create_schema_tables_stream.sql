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
go

CREATE SCHEMA tests;
go

-- create tables
CREATE TABLE tests.samples (
	sample_id INT IDENTITY (1, 1) PRIMARY KEY,
	sample_name VARCHAR (255) NOT NULL
);
go

CREATE TABLE tests.files (
	fstream_id UNIQUEIDENTIFIER ROWGUIDCOL NOT NULL UNIQUE,
	fstream_name VARCHAR (255) NOT NULL,
	fstream_binary VARBINARY(MAX) FILESTREAM NULL
);
go
