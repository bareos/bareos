-- Catalog-only accounting samples from the anonymized Bareos fixtures.
-- This creates synthetic Client, FileSet, Job, Path, and File records only;
-- it does not create Media/JobMedia rows or backup payloads and cannot be used
-- to restore these samples. Review the fixture names before running on a live
-- catalog. Re-running this script updates the fixture catalog rows in place.
-- For visible report values, Windows and NDMP regular-file LStat sizes/blocks
-- and NDMP plus MSSQL JobBytes/ReadBytes are scaled by 4096x. This is
-- synthetic display data, not source size.
--
-- Run with psql against the target catalog, for example:
--   psql -X -v ON_ERROR_STOP=1 -d bareos -f accounting-fixtures-seed.sql

BEGIN;

CREATE TEMP TABLE seed_clients (
  name TEXT PRIMARY KEY,
  uname TEXT NOT NULL
) ON COMMIT DROP;
CREATE TEMP TABLE seed_filesets (
  client_name TEXT NOT NULL,
  fileset_name TEXT NOT NULL,
  fileset_text TEXT NOT NULL,
  md5 TEXT NOT NULL,
  PRIMARY KEY (client_name, fileset_name)
) ON COMMIT DROP;
CREATE TEMP TABLE seed_jobs (
  client_name TEXT NOT NULL,
  fileset_name TEXT NOT NULL,
  job_name TEXT PRIMARY KEY,
  type CHAR(1) NOT NULL,
  level CHAR(1) NOT NULL,
  jobstatus CHAR(1) NOT NULL,
  jobfiles INTEGER NOT NULL,
  jobbytes BIGINT NOT NULL,
  readbytes BIGINT NOT NULL,
  age_seconds INTEGER NOT NULL
) ON COMMIT DROP;
CREATE TEMP TABLE seed_files (
  job_name TEXT NOT NULL,
  path TEXT NOT NULL,
  fileindex INTEGER NOT NULL,
  name TEXT NOT NULL,
  lstat TEXT NOT NULL,
  md5 TEXT NOT NULL,
  deltaseq SMALLINT NOT NULL,
  fhinfo NUMERIC(20) NOT NULL,
  fhnode NUMERIC(20) NOT NULL
) ON COMMIT DROP;

INSERT INTO seed_clients (name, uname) VALUES
  ('acct-barri-fixture', 'Windows'),
  ('acct-windows-fixture', 'Windows Server'),
  ('acct-mssql-fixture', 'Windows Server'),
  ('acct-vmware-fixture', 'Linux'),
  ('acct-hyperv-fixture', 'Windows Server'),
  ('acct-ndmp-bareos', ''),
  ('acct-ndmp-native', '');

INSERT INTO seed_filesets (client_name, fileset_name, fileset_text, md5) VALUES
  ('acct-barri-fixture', 'AcctBarriFixture', 'FileSet {
  Name = "AcctBarriFixture"
  Include {
    Plugin = "barri:"
  }
}
', '0'),
  ('acct-windows-fixture', 'AcctWindowsFixture', 'FileSet {
  Name = "AcctWindowsFixture"
  Include {
    File = "/accounting-fixtures/windows/"
  }
}
', '0'),
  ('acct-mssql-fixture', 'AcctMssqlNormal', 'FileSet {
  Name = "AcctMssqlNormal"
  Include {
    Plugin = "mssqlvdi:"
  }
}
', '0'),
  ('acct-mssql-fixture', 'AcctMssqlFileStream', 'FileSet {
  Name = "AcctMssqlFileStream"
  Include {
    Plugin = "mssqlvdi:"
  }
}
', '0'),
  ('acct-vmware-fixture', 'AcctVmwareFixtureA', 'FileSet {
  Name = "AcctVmwareFixtureA"
  Include {
    Plugin = "python:module_name=bareos-fd-vmware"
  }
}
', '0'),
  ('acct-vmware-fixture', 'AcctVmwareFixtureB', 'FileSet {
  Name = "AcctVmwareFixtureB"
  Include {
    Plugin = "python:module_name=bareos-fd-vmware"
  }
}
', '0'),
  ('acct-vmware-fixture', 'AcctVmwareFixtureC', 'FileSet {
  Name = "AcctVmwareFixtureC"
  Include {
    Plugin = "python:module_name=bareos-fd-vmware"
  }
}
', '0'),
  ('acct-hyperv-fixture', 'AcctHypervFixture', 'FileSet {
  Name = "AcctHypervFixture"
  Include {
    Plugin = "hyper-v:vmname=fixture-vm"
  }
}
', '0'),
  ('acct-ndmp-bareos', 'AcctNdmpBareos', 'FileSet {
  Name = "ndmp-fileset"
  Include {
    Options {
      HardLinks = No
      AclSupport = Yes
      XattrSupport = Yes
      Meta = "HIST=F"
      Meta = "DIRECT=N"
      Meta = "UPDATE=Y"
      Meta = "BUTYPE=DUMP"
    }
    File = "/ndmp-accounting/bareos"
  }
}

', '06/Fc3YEX5/goWZjN8FHvC'),
  ('acct-ndmp-native', 'AcctNdmpNative', 'FileSet {
  Name = "ndmp-fileset"
  Include {
    Options {
      HardLinks = No
      AclSupport = Yes
      XattrSupport = Yes
      Meta = "HIST=F"
      Meta = "DIRECT=N"
      Meta = "UPDATE=Y"
      Meta = "BUTYPE=DUMP"
    }
    File = "/ndmp-accounting/native"
  }
}

', 'M5Fov7xDZ+A/zj/E4nMORA');

INSERT INTO seed_jobs (
  client_name, fileset_name, job_name, type, level, jobstatus,
  jobfiles, jobbytes, readbytes, age_seconds
) VALUES
  ('acct-barri-fixture', 'AcctBarriFixture', 'acct-barri-fixture-AcctBarriFixture-F', 'B', 'F', 'T', 2, 8874264928, 15097253018, 30),
  ('acct-windows-fixture', 'AcctWindowsFixture', 'acct-windows-fixture-AcctWindowsFixture-F', 'B', 'F', 'T', 124434, 14774011814, 14774011814, 30),
  ('acct-windows-fixture', 'AcctWindowsFixture', 'acct-windows-fixture-AcctWindowsFixture-I', 'B', 'I', 'T', 3099, 1172232107, 1172232107, 0),
  ('acct-mssql-fixture', 'AcctMssqlNormal', 'acct-mssql-fixture-AcctMssqlNormal-F', 'B', 'F', 'T', 1, 16106127360, 16106127360, 30),
  ('acct-mssql-fixture', 'AcctMssqlNormal', 'acct-mssql-fixture-AcctMssqlNormal-I', 'B', 'I', 'T', 1, 2684354560, 2684354560, 0),
  ('acct-mssql-fixture', 'AcctMssqlFileStream', 'acct-mssql-fixture-AcctMssqlFileStream-F', 'B', 'F', 'T', 1, 18522046464, 18522046464, 30),
  ('acct-mssql-fixture', 'AcctMssqlFileStream', 'acct-mssql-fixture-AcctMssqlFileStream-I', 'B', 'I', 'T', 1, 3489660928, 3489660928, 0),
  ('acct-vmware-fixture', 'AcctVmwareFixtureA', 'acct-vmware-fixture-AcctVmwareFixtureA-F', 'B', 'F', 'T', 6, 2678079734, 2678079734, 30),
  ('acct-vmware-fixture', 'AcctVmwareFixtureB', 'acct-vmware-fixture-AcctVmwareFixtureB-F', 'B', 'F', 'T', 6, 3234873847, 3234873847, 30),
  ('acct-vmware-fixture', 'AcctVmwareFixtureC', 'acct-vmware-fixture-AcctVmwareFixtureC-F', 'B', 'F', 'T', 6, 2540715711, 2540715711, 30),
  ('acct-hyperv-fixture', 'AcctHypervFixture', 'acct-hyperv-fixture-AcctHypervFixture-F', 'B', 'F', 'T', 7, 1166220216, 10741703630, 30),
  ('acct-hyperv-fixture', 'AcctHypervFixture', 'acct-hyperv-fixture-AcctHypervFixture-I', 'B', 'I', 'T', 7, 260726, 4334574, 0),
  ('acct-ndmp-bareos', 'AcctNdmpBareos', 'acct-ndmp-bareos-AcctNdmpBareos-F', 'B', 'F', 'T', 8, 41943040, 0, 30),
  ('acct-ndmp-bareos', 'AcctNdmpBareos', 'acct-ndmp-bareos-AcctNdmpBareos-I', 'B', 'I', 'T', 4, 293601280, 0, 0),
  ('acct-ndmp-native', 'AcctNdmpNative', 'acct-ndmp-native-AcctNdmpNative-F', 'B', 'F', 'T', 8, 41943040, 0, 30),
  ('acct-ndmp-native', 'AcctNdmpNative', 'acct-ndmp-native-AcctNdmpNative-I', 'B', 'I', 'T', 4, 293601280, 0, 0);

INSERT INTO seed_files (
  job_name, path, fileindex, name, lstat, md5, deltaseq, fhinfo, fhnode
) VALUES
  ('acct-barri-fixture-AcctBarriFixture-F', '/accounting-fixtures/barri/65eba12ff5a78377/', 1, 'entry', 'A A IHA A A A A -B BAA B BqxDhN BqxDhN BqxDhN A A d', '0', 0, 0, 0),
  ('acct-barri-fixture-AcctBarriFixture-F', '/accounting-fixtures/barri/646f7ae4b1f78659/', 2, 'entry', 'A A IHA A A A A LR7H BAA LS BqxDhN BqxDhN BqxDhN A A d', '0', 0, 0, 0),
  ('acct-windows-fixture-AcctWindowsFixture-F', '/accounting-fixtures/windows/44d2d3e63679ba90/', 2604, 'entry', 'CgxRG2 BAAAAAXrP IH/ B A A g DGAA BAA BAA BqV7jR BqV7jR BqVvCT A A L', '0', 0, 0, 0),
  ('acct-windows-fixture-AcctWindowsFixture-F', '/accounting-fixtures/windows/32d0d0daa0281e43/', 67877, 'entry', 'CgxRG2 BAAAAAIUW IH/ C A A g UAAAA BAA UAA BglkiA BglkiA BglkiA A A L', '0', 0, 0, 0),
  ('acct-windows-fixture-AcctWindowsFixture-F', '/accounting-fixtures/windows/06b8160036ac4a56/', 2606, 'entry', 'CgxRG2 BAAAAAXrC IH/ B A A g A BAA A BqVvCT BqVvCT BqVvCT A A L', '0', 0, 0, 0),
  ('acct-windows-fixture-AcctWindowsFixture-F', '/accounting-fixtures/windows/db6216a7072e8ac0/', 51969, 'entry', 'CgxRG2 BAAAAATDl IH/ E A A g 4AAA BAA BAA Bgllsz Bgllsz Bgllsz A A L', '0', 0, 0, 0),
  ('acct-windows-fixture-AcctWindowsFixture-F', '/accounting-fixtures/windows/28fb3d095626632d/', 67878, 'entry', 'A A EH/ A A A Q A BAA A BglkiB BglkiB BqVv4t A A L', '0', 0, 0, 0),
  ('acct-windows-fixture-AcctWindowsFixture-F', '/accounting-fixtures/windows/bf635be166079ca0/', 3769, 'entry', 'A A KH/ A A A gACQW A BAA A BqVvCR BqVvCR BqVvCR A A L', '0', 0, 0, 0),
  ('acct-windows-fixture-AcctWindowsFixture-I', '/accounting-fixtures/windows/44d2d3e63679ba90/', 883, 'entry', 'CgxRG2 BAAAAAXrP IH/ B A A g ESAA BAA BAA BqWDyW BqWDyW BqVvCT A A L', '0', 0, 0, 0),
  ('acct-windows-fixture-AcctWindowsFixture-I', '/accounting-fixtures/windows/c985331111c2ca4f/', 2816, 'entry', 'CgxRG2 IAAAAAYDe IH/ B A A CAg EAAAA BAA EAA BqWCQw BqWCQw BqWCP0 A A L', '0', 0, 0, 0),
  ('acct-mssql-fixture-AcctMssqlNormal-F', '/@MSSQL/default/AcctMssqlNormal/', 1, 'full.bak', 'A A IHA A A A A A QAA B BqxiHX BqxiHX BqxiHX A A L', '0', 0, 0, 0),
  ('acct-mssql-fixture-AcctMssqlNormal-I', '/@MSSQL/default/AcctMssqlNormal/', 1, 'log.trn', 'A A IHA A A A A A QAA B BqxiHi BqxiHi BqxiHi A A L', '0', 0, 0, 0),
  ('acct-mssql-fixture-AcctMssqlFileStream-F', '/@MSSQL/default/AcctMssqlFileStream/', 1, 'full.bak', 'A A IHA A A A A A QAA B BqxiIC BqxiIC BqxiIC A A L', '0', 0, 0, 0),
  ('acct-mssql-fixture-AcctMssqlFileStream-I', '/@MSSQL/default/AcctMssqlFileStream/', 1, 'log.trn', 'A A IHA A A A A A QAA B BqxiIO BqxiIO BqxiIO A A L', '0', 0, 0, 0),
  ('acct-vmware-fixture-AcctVmwareFixtureA-F', '/accounting-fixtures/vmware/4f17a02cd23f986e/', 4, 'guest.nvram', 'A A IHA A A A A -B BAA B BqxDhN BqxDhN BqxDhN A A d', '0', 0, 0, 0),
  ('acct-vmware-fixture-AcctVmwareFixtureA-F', '/accounting-fixtures/vmware/4f17a02cd23f986e/', 6, 'guest.vmdk', 'A A IHA A A A A -B BAA B BqxDhN BqxDhN BqxDhN A A d', '0', 0, 0, 0),
  ('acct-vmware-fixture-AcctVmwareFixtureB-F', '/accounting-fixtures/vmware/a30ef45ae89e4a76/', 4, 'guest.nvram', 'A A IHA A A A A -B BAA B BqxDhN BqxDhN BqxDhN A A d', '0', 0, 0, 0),
  ('acct-vmware-fixture-AcctVmwareFixtureB-F', '/accounting-fixtures/vmware/a30ef45ae89e4a76/', 6, 'guest.vmdk', 'A A IHA A A A A -B BAA B BqxDhN BqxDhN BqxDhN A A d', '0', 0, 0, 0),
  ('acct-vmware-fixture-AcctVmwareFixtureC-F', '/accounting-fixtures/vmware/98061845d52b320a/', 4, 'guest.nvram', 'A A IHA A A A A -B BAA B BqxDhN BqxDhN BqxDhN A A d', '0', 0, 0, 0),
  ('acct-vmware-fixture-AcctVmwareFixtureC-F', '/accounting-fixtures/vmware/98061845d52b320a/', 6, 'guest.vmdk', 'A A IHA A A A A -B BAA B BqxDhN BqxDhN BqxDhN A A d', '0', 0, 0, 0),
  ('acct-hyperv-fixture-AcctHypervFixture-F', '/accounting-fixtures/hyperv/vm-config/', 1, 'fixture-vm.vmrs', 'A A IHA A A A A -B BAA B BqxjDT BqxjDT BqxjDT A A f', '0', 0, 0, 0),
  ('acct-hyperv-fixture-AcctHypervFixture-F', '/accounting-fixtures/hyperv/vm-config/', 2, 'fixture-vm.vmgs', 'A A IHA A A A A -B BAA B BqxjDT BqxjDT BqxjDT A A f', '0', 0, 0, 0),
  ('acct-hyperv-fixture-AcctHypervFixture-F', '/accounting-fixtures/hyperv/vm-config/', 3, 'fixture-vm.vmcx', 'A A IHA A A A A -B BAA B BqxjDT BqxjDT BqxjDT A A f', '0', 0, 0, 0),
  ('acct-hyperv-fixture-AcctHypervFixture-F', '/accounting-fixtures/hyperv/virtual-disk/', 4, 'fixture-disk.vhdx', 'A A IHA A A A A KAAAAA CAAAA IA BqxjDT BqxjDT BqxjDT A A f', '0', 0, 0, 0),
  ('acct-hyperv-fixture-AcctHypervFixture-I', '/accounting-fixtures/hyperv/vm-config/', 1, 'fixture-vm.vmrs', 'A A IHA A A A A -B BAA B BqxjFO BqxjFO BqxjFO A A f', '0', 0, 0, 0),
  ('acct-hyperv-fixture-AcctHypervFixture-I', '/accounting-fixtures/hyperv/vm-config/', 2, 'fixture-vm.vmgs', 'A A IHA A A A A -B BAA B BqxjFO BqxjFO BqxjFO A A f', '0', 0, 0, 0),
  ('acct-hyperv-fixture-AcctHypervFixture-I', '/accounting-fixtures/hyperv/vm-config/', 3, 'fixture-vm.vmcx', 'A A IHA A A A A -B BAA B BqxjFO BqxjFO BqxjFO A A f', '0', 0, 0, 0),
  ('acct-hyperv-fixture-AcctHypervFixture-I', '/accounting-fixtures/hyperv/virtual-disk/', 4, 'fixture-disk.vhdx', 'A A IHA A A A A KAAAAA IAAA IA BqxjFO BqxjFO BqxjFO A A f', '0', 1, 0, 0),
  ('acct-ndmp-bareos-AcctNdmpBareos-F', '/ndmp-accounting/bareos/', 1, 'empty', 'A A IGk A Pp Pp A A IA A Bqxdlx Bqxdlx Bqxdlx B A B', '0', 0, 0, 0),
  ('acct-ndmp-bareos-AcctNdmpBareos-F', '/ndmp-accounting/bareos/', 1, 'full', 'A A IGk QB Pp Pp A QBAA IA DAA Bqxdlx Bqxdlx Bqxdlx B A B', '0', 0, 0, 0),
  ('acct-ndmp-bareos-AcctNdmpBareos-F', '/ndmp-accounting/bareos/sub/', 1, '', 'A A EHt d Pp Pp A d IA B Bqxdlx Bqxdlx Bqxdlx B A B', '0', 0, 0, 0),
  ('acct-ndmp-bareos-AcctNdmpBareos-F', '/ndmp-accounting/bareos/sub/dir/', 1, '', 'A A EHt S Pp Pp A S IA B Bqxdlx Bqxdlx Bqxdlx B A B', '0', 0, 0, 0),
  ('acct-ndmp-bareos-AcctNdmpBareos-F', '/ndmp-accounting/bareos/sub/dir/', 1, 'file', 'A A IGk Po Pp Pp A PoAA IA CAA Bqxdlx Bqxdlx Bqxdlx B A B', '0', 0, 0, 0),
  ('acct-ndmp-bareos-AcctNdmpBareos-F', '/ndmp-accounting/bareos/sub/', 1, 'link', 'A A KH/ L Pp Pp A L IA B Bqxdlx Bqxdlx Bqxdlx B A B', '0', 0, 0, 0),
  ('acct-ndmp-bareos-AcctNdmpBareos-F', '/ndmp-accounting/bareos/', 1, 'testfile', 'A A IHt B Pp Pp A BAA IA BAA Bqxdlx Bqxdlx Bqxdlx B A B', '0', 0, 0, 0),
  ('acct-ndmp-bareos-AcctNdmpBareos-F', '/ndmp-accounting/bareos/', 1, 'testfile2', 'A A IHt IA Pp Pp A IAAA IA BAA Bqxdlx Bqxdlx Bqxdlx B A B', '0', 0, 0, 0),
  ('acct-ndmp-bareos-AcctNdmpBareos-F', '/@NDMP/ndmp-accounting/', 1, 'bareos%0', 'A A IHA A A A A -B BAA B Bqxdl0 Bqxdl0 Bqxdl0 B A B', '0', 0, 0, 0),
  ('acct-ndmp-bareos-AcctNdmpBareos-I', '/ndmp-accounting/bareos/', 1, 'incremental', 'A A IGk IB Pp Pp A IBAA IA CAA Bqxdl4 Bqxdl4 Bqxdl4 B A B', '0', 0, 0, 0),
  ('acct-ndmp-bareos-AcctNdmpBareos-I', '/ndmp-accounting/bareos/sub/', 1, '', 'A A EHt d Pp Pp A d IA B Bqxdlx Bqxdlx Bqxdlx B A B', '0', 0, 0, 0),
  ('acct-ndmp-bareos-AcctNdmpBareos-I', '/ndmp-accounting/bareos/sub/dir/', 1, '', 'A A EHt S Pp Pp A S IA B Bqxdlx Bqxdlx Bqxdlx B A B', '0', 0, 0, 0),
  ('acct-ndmp-bareos-AcctNdmpBareos-I', '/ndmp-accounting/bareos/sub/dir/', 1, 'file', 'A A IGk QAB Pp Pp A QABAA IA CBAA Bqxdl0 Bqxdl4 Bqxdl4 B A B', '0', 0, 0, 0),
  ('acct-ndmp-bareos-AcctNdmpBareos-I', '/@NDMP/ndmp-accounting/', 1, 'bareos%1', 'A A IHA A A A A -B BAA B Bqxdl5 Bqxdl5 Bqxdl5 B A B', '0', 0, 0, 0),
  ('acct-ndmp-native-AcctNdmpNative-F', '/ndmp-accounting/native/', 1, 'empty', 'A A IGk A Pp Pp A A IA A BqxdmA BqxdmA BqxdmA B A B', '0', 0, 512, 80555457),
  ('acct-ndmp-native-AcctNdmpNative-F', '/ndmp-accounting/native/', 1, 'full', 'A A IGk QB Pp Pp A QBAA IA DAA Bqxdl+ BqxdmA BqxdmA B A B', '0', 0, 1024, 80571780),
  ('acct-ndmp-native-AcctNdmpNative-F', '/ndmp-accounting/native/sub/', 1, '', 'A A EHt d Pp Pp A d IA B Bqxdl+ Bqxdl+ Bqxdl+ B A B', '0', 0, 3072, 551422092),
  ('acct-ndmp-native-AcctNdmpNative-F', '/ndmp-accounting/native/sub/dir/', 1, '', 'A A EHt S Pp Pp A S IA B Bqxdl+ Bqxdl+ Bqxdl+ B A B', '0', 0, 3584, 1074824607),
  ('acct-ndmp-native-AcctNdmpNative-F', '/ndmp-accounting/native/sub/dir/', 1, 'file', 'A A IGk Po Pp Pp A PoAA IA CAA Bqxdl+ BqxdmA BqxdmA B A B', '0', 0, 4096, 1075647978),
  ('acct-ndmp-native-AcctNdmpNative-F', '/ndmp-accounting/native/sub/', 1, 'link', 'A A KH/ L Pp Pp A L IA B Bqxdl+ Bqxdl+ Bqxdl+ B A B', '0', 0, 5632, 551485322),
  ('acct-ndmp-native-AcctNdmpNative-F', '/ndmp-accounting/native/', 1, 'testfile', 'A A IHt B Pp Pp A BAA IA BAA Bqxdl+ BqxdmA BqxdmA B A B', '0', 0, 6144, 80571781),
  ('acct-ndmp-native-AcctNdmpNative-F', '/ndmp-accounting/native/', 1, 'testfile2', 'A A IHt IA Pp Pp A IAAA IA BAA Bqxdl+ BqxdmA BqxdmA B A B', '0', 0, 7168, 80571782),
  ('acct-ndmp-native-AcctNdmpNative-I', '/ndmp-accounting/native/', 1, 'incremental', 'A A IGk IB Pp Pp A IBAA IA CAA BqxdmH BqxdmH BqxdmH B A B', '0', 0, 512, 80555459),
  ('acct-ndmp-native-AcctNdmpNative-I', '/ndmp-accounting/native/sub/', 1, '', 'A A EHt d Pp Pp A d IA B Bqxdl+ Bqxdl+ Bqxdl+ B A B', '0', 0, 2048, 551422092),
  ('acct-ndmp-native-AcctNdmpNative-I', '/ndmp-accounting/native/sub/dir/', 1, '', 'A A EHt S Pp Pp A S IA B Bqxdl+ Bqxdl+ Bqxdl+ B A B', '0', 0, 2560, 1074824607),
  ('acct-ndmp-native-AcctNdmpNative-I', '/ndmp-accounting/native/sub/dir/', 1, 'file', 'A A IGk QAB Pp Pp A QABAA IA CBAA BqxdmD BqxdmH BqxdmH B A B', '0', 0, 3072, 1075647978);

INSERT INTO Client (Name, UName)
SELECT name, uname FROM seed_clients
ON CONFLICT (Name) DO NOTHING;

INSERT INTO FileSet (FileSet, FileSetText, Md5, CreateTime)
SELECT s.fileset_name, s.fileset_text, s.md5, transaction_timestamp()
FROM seed_filesets s
WHERE NOT EXISTS (
  SELECT 1 FROM FileSet f
  WHERE f.FileSet = s.fileset_name
    AND f.FileSetText = s.fileset_text
    AND f.Md5 = s.md5
);

INSERT INTO Job (
  Job, Name, Type, Level, ClientId, JobStatus, SchedTime, StartTime,
  EndTime, RealEndTime, JobTDate, JobFiles, JobBytes, ReadBytes, FileSetId
)
SELECT
  s.job_name, s.job_name, s.type, s.level, c.ClientId, s.jobstatus,
  transaction_timestamp() - make_interval(secs => s.age_seconds + 2),
  transaction_timestamp() - make_interval(secs => s.age_seconds + 2),
  transaction_timestamp() - make_interval(secs => s.age_seconds),
  transaction_timestamp() - make_interval(secs => s.age_seconds),
  EXTRACT(EPOCH FROM transaction_timestamp() - make_interval(secs => s.age_seconds))::BIGINT,
  s.jobfiles, s.jobbytes, s.readbytes, f.FileSetId
FROM seed_jobs s
JOIN Client c ON c.Name = s.client_name
JOIN LATERAL (
  SELECT fs.FileSetId
  FROM FileSet fs
  JOIN seed_filesets sf
    ON sf.fileset_name = fs.FileSet
   AND sf.fileset_text = fs.FileSetText
   AND sf.md5 = fs.Md5
  WHERE sf.client_name = s.client_name
    AND sf.fileset_name = s.fileset_name
  ORDER BY fs.FileSetId
  LIMIT 1
) f ON TRUE
WHERE NOT EXISTS (
  SELECT 1 FROM Job j
  WHERE j.Job = s.job_name
    AND j.ClientId = c.ClientId
    AND j.FileSetId = f.FileSetId
    AND j.Level = s.level
);

UPDATE Job j
SET Type = s.type,
    Level = s.level,
    JobStatus = s.jobstatus,
    SchedTime = transaction_timestamp() - make_interval(secs => s.age_seconds + 2),
    StartTime = transaction_timestamp() - make_interval(secs => s.age_seconds + 2),
    EndTime = transaction_timestamp() - make_interval(secs => s.age_seconds),
    RealEndTime = transaction_timestamp() - make_interval(secs => s.age_seconds),
    JobTDate = EXTRACT(
      EPOCH FROM transaction_timestamp() - make_interval(secs => s.age_seconds)
    )::BIGINT,
    JobFiles = s.jobfiles,
    JobBytes = s.jobbytes,
    ReadBytes = s.readbytes
FROM seed_jobs s
JOIN Client c ON c.Name = s.client_name
JOIN LATERAL (
  SELECT fs.FileSetId
  FROM FileSet fs
  JOIN seed_filesets sf
    ON sf.fileset_name = fs.FileSet
   AND sf.fileset_text = fs.FileSetText
   AND sf.md5 = fs.Md5
  WHERE sf.client_name = s.client_name
    AND sf.fileset_name = s.fileset_name
  ORDER BY fs.FileSetId
  LIMIT 1
) fs ON TRUE
WHERE j.Job = s.job_name
  AND j.ClientId = c.ClientId
  AND j.FileSetId = fs.FileSetId
  AND j.Level = s.level;

INSERT INTO Path (Path)
SELECT DISTINCT s.path
FROM seed_files s
WHERE NOT EXISTS (SELECT 1 FROM Path p WHERE p.Path = s.path);

INSERT INTO File (JobId, PathId, FileIndex, Name, LStat, Md5, DeltaSeq, Fhinfo, Fhnode)
SELECT j.JobId, p.PathId, s.fileindex, s.name, s.lstat, s.md5,
       s.deltaseq, s.fhinfo, s.fhnode
FROM seed_files s
JOIN seed_jobs sj ON sj.job_name = s.job_name
JOIN Client c ON c.Name = sj.client_name
JOIN seed_filesets sf
  ON sf.client_name = sj.client_name
 AND sf.fileset_name = sj.fileset_name
JOIN FileSet fs
  ON fs.FileSet = sf.fileset_name
 AND fs.FileSetText = sf.fileset_text
 AND fs.Md5 = sf.md5
JOIN Job j
  ON j.Job = sj.job_name
 AND j.ClientId = c.ClientId
 AND j.Level = sj.level
 AND j.FileSetId = fs.FileSetId
JOIN Path p ON p.Path = s.path
WHERE NOT EXISTS (
  SELECT 1 FROM File f
  WHERE f.JobId = j.JobId
    AND f.PathId = p.PathId
    AND f.FileIndex = s.fileindex
    AND f.Name = s.name
);

UPDATE File f
SET LStat = s.lstat,
    Md5 = s.md5,
    DeltaSeq = s.deltaseq,
    Fhinfo = s.fhinfo,
    Fhnode = s.fhnode
FROM seed_files s
JOIN seed_jobs sj ON sj.job_name = s.job_name
JOIN Client c ON c.Name = sj.client_name
JOIN seed_filesets sf
  ON sf.client_name = sj.client_name
 AND sf.fileset_name = sj.fileset_name
JOIN FileSet fs
  ON fs.FileSet = sf.fileset_name
 AND fs.FileSetText = sf.fileset_text
 AND fs.Md5 = sf.md5
JOIN Job j
  ON j.Job = sj.job_name
 AND j.ClientId = c.ClientId
 AND j.Level = sj.level
 AND j.FileSetId = fs.FileSetId
JOIN Path p ON p.Path = s.path
WHERE f.JobId = j.JobId
  AND f.PathId = p.PathId
  AND f.FileIndex = s.fileindex
  AND f.Name = s.name;

SELECT c.Name AS client, fs.FileSet, j.Level, COUNT(DISTINCT j.JobId) AS jobs,
       COUNT(f.FileId) AS imported_file_rows
FROM seed_clients sc
JOIN Client c ON c.Name = sc.name
JOIN Job j ON j.ClientId = c.ClientId
JOIN FileSet fs ON fs.FileSetId = j.FileSetId
LEFT JOIN File f ON f.JobId = j.JobId
GROUP BY c.Name, fs.FileSet, j.Level
ORDER BY c.Name, fs.FileSet, j.Level;

COMMIT;
