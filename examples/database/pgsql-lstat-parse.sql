--
-- Make sure we have plpgsql available.
--
CREATE LANGUAGE plpgsql;

--
-- Stored procedure to convert an base64 string into an integer
--
CREATE FUNCTION bacula_base64(field text) RETURNS INTEGER AS $$
DECLARE i INTEGER DEFAULT 1;
        res INTEGER DEFAULT 0;
        base64 TEXT DEFAULT 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';
BEGIN
   WHILE i <= char_length(field) LOOP
      res := res << 6;
      res := res + position(substring(field from i for 1) in base64) - 1;
      i :=  i + 1;
   END LOOP;
   RETURN res;
END $$ LANGUAGE 'plpgsql' IMMUTABLE;

--
-- This table never contains any data, it's just a definition for the row we'll return.
--
CREATE TABLE lstat (
   st_dev INTEGER,
   st_ino INTEGER,
   st_mode INTEGER,
   st_nlink INTEGER,
   st_uid INTEGER,
   st_gid INTEGER,
   st_rdev INTEGER,
   st_size INTEGER,
   st_blksize INTEGER,
   st_blocks INTEGER,
   st_atime INTEGER,
   st_mtime INTEGER,
   st_ctime INTEGER,
   LinkFI INTEGER,
   st_flags INTEGER,
   data INTEGER
);

--
-- Stored procedure in plpgsql that splits the lstat fields.
--
CREATE FUNCTION decode_lstat(lst text) RETURNS lstat AS $$
DECLARE st lstat%ROWTYPE;
        ret record;
        fields text[];
BEGIN
   fields = regexp_split_to_array(lst, ' ');
   st.st_dev := bacula_base64(fields[1]);
   st.st_ino := bacula_base64(fields[2]);
   st.st_mode := bacula_base64(fields[3]);
   st.st_nlink := bacula_base64(fields[4]);
   st.st_uid := bacula_base64(fields[5]);
   st.st_gid := bacula_base64(fields[6]);
   st.st_rdev := bacula_base64(fields[7]);
   st.st_size := bacula_base64(fields[8]);
   st.st_blksize := bacula_base64(fields[9]);
   st.st_blocks := bacula_base64(fields[10]);
   st.st_atime := bacula_base64(fields[11]);
   st.st_mtime := bacula_base64(fields[12]);
   st.st_ctime := bacula_base64(fields[13]);
   st.LinkFI := bacula_base64(fields[14]);
   st.st_flags := bacula_base64(fields[15]);
   st.data := bacula_base64(fields[16]);
   RETURN st;
END $$ LANGUAGE 'plpgsql' IMMUTABLE;

--
-- SELECT (SELECT st_size FROM decode_lstat(File.lstat)) AS Size, Path.Name, FileName.Name
-- FROM File,FileName,Path
-- WHERE File.JobId = '%1'
-- AND Filename.FileNameId = File.FileNameid
-- AND File.PathId = Path.PathId
-- ORDER by Size DESC
-- LIMIT 20;
--
