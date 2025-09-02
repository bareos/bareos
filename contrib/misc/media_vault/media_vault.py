#!/usr/bin/env python3
""" Bareos Module for Media Vault
    Define and search a list of tapes to vault
    Eject that list up to import/export slots available
"""
#   BAREOS - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2023-2025 Bareos GmbH & Co. KG
#
#   This program is Free Software; you can redistribute it and/or
#   modify it under the terms of version three of the GNU Affero General Public
#   License as published by the Free Software Foundation and included
#   in the file LICENSE.
#
#   This program is distributed in the hope that it will be useful, but
#   WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
#   Affero General Public License for more details.
#
#   You should have received a copy of the GNU Affero General Public License
#   along with this program; if not, write to the Free Software
#   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
#   02110-1301, USA.

# -*- coding: utf-8 -*-

import logging
import logging.config
import os
import sys
import hashlib
from io import StringIO

try:
    from pathlib import Path
    import datetime
    import configargparse
    import bareos.bsock
    import bareos.exceptions
except ImportError as err_import:
    print(f"Error: missing module please install: {err_import})\n")
except Exception as err_unknown:
    print(f"Fatal: Unknown error {err_unknown}\n")



def filter_maker(level):
    """ global redefinition for filtering lower log level """
    level = getattr(logging, level)
    def filter(record):
        return record.levelno <= level
    return filter

# Prepare a global logger which is not the root one.
me = Path(__file__).stem
# Better use dictConfig than basicConfig.
logging_config = {
    "version" : 1,
    "disable_existing_logger" : False,
    "level": "WARNING",
    "formatters" : {
        "simple" : {
            "format" : '%(asctime)s: %(levelname)s %(module)s.%(funcName)s: %(message)s',
        }
    },
    "filters": {
        "warnings_and_below": {
            "()" : "__main__.filter_maker",
            "level": "WARNING"
        },
    },
    "handlers" : {
        "stdout":{
            "class": "logging.StreamHandler",
            "formatter": "simple",
            "filters": ["warnings_and_below"],
            "stream": "ext://sys.stdout",
        },
        "stderr":{
            "class": "logging.StreamHandler",
            "level": "ERROR",
            "formatter": "simple",
            "stream": "ext://sys.stderr",
        },
    },
    "loggers":{
        "root": {"level": "INFO", "handlers":["stderr"]},
        f"{me}": {"level": "WARNING", "handlers":["stdout","stderr"]}
    },
}

logging.config.dictConfig(config=logging_config)
logger = logging.getLogger(me)

class MediaVault():
    """ Class used for volume export management """
    def __init__(self):
        logger.debug("init start")
        now = datetime.datetime.now()
        # DirectorConsoleConnectionJson the main usage
        self.dcj = None
        # Script arguments call
        self.args = None
        self.bareos_args = None
        self.config_file = None
        self.config_parsed = False

        self.sql = None
        self.sql_result = None
        # List of pools considered for exports
        self.pools = []
        # List of volumes to export
        self.volumes_candidates = []
        self.volumes_exported = []
        # bareos-dir storage autochanger name
        self.autochanger = None
        # exp/imp slots range in notation 20-24
        self.export_slots = None
        # minimal bytes on volumes before exporting default 4TB
        self.min_volbytes = 4096*1024*1024*1024
        # This flag if present will order to run update slots before any other action
        self.update_slots_before = True
        # This flag if present will order to update the volstatus to Archive
        self.volstatus_to_archive = False
        # maximal back days to look for (max_age <= vol.lastwritten >= min_age)
        self.max_age = 21
        # minimum back days to consider for exporting
        self.min_age = 1
        # default return date expected.
        self.return_time = 90
        # default list of volume status to consider
        self.volume_status = ["Full","Used"]
        # format need to be a valid format used by postgresql to_char() function
        self.returndate_format = 'YYYY-MM-DD'
        self.log_dir = None
        self.ftp_dir = None
        self.ftp_file = None
        self.iron_acct = None
        self.iron_name = None
        self.date_time = now.strftime("%Y%m%d-%H%M%S")
        self.vault_time = now.strftime("%Y-%m-%d, %H:%M:%S")
        logger.debug("init end")


    def _check_cli_args(self):
        """cli argument checks"""
        if self.args.debug:
            # if log_dir exist we create a debug log
            if self.args.log_dir:
                self.log_dir = self.args.log_dir
                if not os.path.isdir(self.log_dir):
                    raise OSError ("log_dir not found")
                log_file = os.path.join( self.log_dir,
                                    f"{me}-{datetime.datetime.now().strftime('%Y%m%d-%H%M%S')}.log"
                                    )
                logger_file = logging.FileHandler(log_file, mode='a', encoding='utf-8', delay=False)
                logger_file.setLevel(logging.DEBUG)
                formatter = logging.Formatter(
                        "%(asctime)s: %(levelname)s "
                        "%(module)s.%(funcName)s: %(message)s"
                )
                logger_file.setFormatter(formatter)
                logger.addHandler(logger_file)
            else:
                raise ValueError ("log_dir need at least one valid path")

        logger.debug("entering _check_cli_args()")
        # at least one pool need to exist
        if self.args.autochanger and len(self.args.autochanger) > 1:
            self.autochanger = self.args.autochanger
        else:
            raise ValueError ("autochanger need at least one valid changer name")

        if self.args.pools:
            self.pools = self.args.pools.split(",")
            if len(self.pools) <= 0:
                raise ValueError("pools need at least one pool name")
        else:
            raise ValueError ("pools need at least one pool name")

        if int(self.args.max_age) >= 1:
            self.max_age = int(self.args.max_age)
        else:
            raise ValueError("max_age need to be a valid integer >= 1")

        if int(self.args.min_age) >= 0:
            self.min_age = int(self.args.min_age)
            if self.max_age <= self.min_age:
                raise ValueError("min_age need to be a valid integer >= 0 and smaller than max_age")

        if int(self.args.return_time) >= 0:
            self.return_time = int(self.args.return_time)
            if self.return_time <= self.min_age:
                raise ValueError("return_time need to be bigger than min_age")

        if self.args.returndate_format and len(self.args.returndate_format) >= 4:
            self.returndate_format = self.args.returndate_format
        else:
            raise ValueError ("returndate_format need at least 4 characters")

        if int(self.args.min_volgb) >= 1:
            self.min_volbytes = int(self.args.min_volgb)*1024*1024*1024
        else:
            raise ValueError("min_volgb need to be a non zero valid integer")

        if self.args.volume_status:
            valid_status = ["Append","Full","Used"]
            vol_status = self.args.volume_status.split(",")
            for status in list(vol_status):
                if status.title() not in valid_status:
                    raise ValueError(
                        f"Volume status {status.title()} is not 'Full','Used' or 'Append'")
                if status.title() not in self.volume_status:
                    self.volume_status.append(status.title())

        if len(self.volume_status) <= 0:
            raise ValueError("Volume status need at least one element in 'Full,Used,Append'")

        if self.args.export_slots and len(self.args.export_slots) > 1:
            self.export_slots = self.args.export_slots
        else:
            raise ValueError ("export_slots need at least one valid slot number")

        self.volstatus_to_archive = self.args.volstatus_to_archive

        self.update_slots_before = self.args.update_slots_before

        if self.args.iron_acct:
            self.iron_acct = self.args.iron_acct

        # we continue iron check only if iron_name is set
        if self.iron_acct:
            if self.args.iron_name:
                self.iron_name = self.args.iron_name
            else:
                raise ValueError ("iron_name has to be a valid name")

            if self.args.ftp_dir:
                self.ftp_dir = self.args.ftp_dir
            else:
                raise ValueError ("ftp-dir need at least one valid path")

            try:
                if not os.path.isdir(self.ftp_dir):
                    os.makedirs(self.ftp_dir)
                    msg = f"OS Info creating {self.ftp_dir}\n"
                    print(msg)
                    logger.debug(msg)
            except OSError as os_error:
                msg_os = f"Fatal ERROR while makedirs('{self.ftp_dir}') \"{os_error}\"\n"
                print(msg_os)
                logger.error(msg_os, exc_info=True)
                raise
            except Exception as err:
                msg_err= f"Unknown Error occurred {err}\n"
                print(msg_err)
                logger.error(msg_err, exc_info=True)
                raise

        logger.debug("leaving _check_cli_args()")


    def _check_valid_storage(self):
        """check if autochanger is know and valid"""
        logger.debug("entering _check_valid_storage()")
        try:
            result = self.dcj.call(f"show storage={self.autochanger}")["storages"]
            if self.autochanger != result[self.autochanger]["name"]:
                raise ValueError(
                        f"autochanger {self.autochanger} is not a valid storage"
                    )
        except bareos.exceptions.Error as berr:
            msg_berr= f"{berr}\n"
            logger.error(msg_berr, exc_info=True)
            raise

        logger.debug("leaving _check_valid_storage()")


    def _check_valid_pools(self):
        """check if pools are known and valid"""
        logger.debug("entering _check_valid_pools()")
        try:
            results = self.dcj.call("show pools")["pools"]
            # Are self.pools contained in larger list pools?
            if not all(element in results for element in self.pools):
                raise ValueError(
                        f"pools {self.pools} are not in pools list ({results})"
                    )
        except bareos.exceptions.Error as berr:
            msg_berr= f"{berr}\n"
            logger.error(msg_berr, exc_info=True)
            raise

        logger.debug("leaving _check_valid_pools()")


    def _bconsole_export_volumes(self):
        """ bconsole export volumes action
            The function build a | separated volume name up to the maximum of export
            slots available and try to eject them all
            If that succeed volume_exported is filed by that list, otherwise stay empty
        """
        logger.debug("entering _bconsole_export_volumes()\n")
        exp_slots = self.export_slots.split('-')
        exp_slots_nb = int(exp_slots[1]) - int(exp_slots[0]) + 1
        export_volnames = ""
        exported_volumes = []
        vol = 0
        total_vols = len(self.volumes_candidates)
        while vol < exp_slots_nb and vol < total_vols:
            volname = self.volumes_candidates[vol]['volumename']
            export_volnames += f"{volname}|"
            exported_volumes.append(self.volumes_candidates[vol])
            vol += 1

        export_volnames = export_volnames.rstrip('|')
        logger.debug("list to export export_volnames='%s'\n",str(export_volnames))

        bc_cmd = (
                f"export volume={export_volnames} storage={self.autochanger} "
                f"dstslots={self.export_slots}"
                )
        logger.debug("boncole command:'%s'\n",bc_cmd)
        try:
            # When successful
            # bytearray(b'{"jsonrpc":"2.0","id":null,"result":{}}')
            res = self.dcj.call(bc_cmd)
            if res == 0:
                logger.debug("bconsole export cmd finish ok'\n")

        except bareos.exceptions.JsonRpcErrorReceivedException as json_exp:
            logger.error("%s\n",json_exp, exc_info=True)
            raise bareos.exceptions.Error(json_exp)
        except bareos.exceptions.Error as berr:
            logger.error("%s\n",berr, exc_info=True)
            raise

        self.volumes_exported = exported_volumes
        logger.debug (
                "_bconsole_export_volumes successful\n"
                "volumes exported: %d\n",len(self.volumes_exported)
            )

        print(f"Successfully exported volumes: {len(self.volumes_exported)}")
        print("Exported volumes:")
        for vol in self.volumes_exported:
            line = f"  {vol['volumename']}"
            print(line)
            logger.debug(line)

        logger.debug("leaving _bconsole_export_volumes()\n")


    def _bconsole_update_slots(self):
        """run the update slots command"""
        logger.debug("entering _bconsole_update_slots()\n")
        try:
            result = self.dcj.call(f"update slots storage={self.autochanger}")
            logger.debug("result:'%s'\n",result)
        except bareos.exceptions.Error as berr:
            logger.error("Update slots error: %s\n",berr, exc_info=True)
            raise

        logger.debug("update slots done\n")
        logger.debug("leaving _bconsole_update_slots()\n")


    def _bconsole_set_volstatus(self):
        """run update volstatus command"""
        logger.debug("entering _bconsole_set_volstatus()\n")
        for vol in self.volumes_exported:
            try:
                result = self.dcj.call(
                        f"update volume={vol['volumename']} volstatus=Archive yes"
                    )
                logger.debug("result:'%s'\n",result)
            except bareos.exceptions.Error as berr:
                logger.error("Set volstatus error: %s\n",berr, exc_info=True)
                raise

        logger.debug("leaving _bconsole_set_volstatus()\n")


    def parse_cli_args(self):
        """ Parse command line arguments """
        logger.debug("entering parse_cli_args()")

        parser = configargparse.ArgParser(
            default_config_files=[f'{Path(__file__).stem}.ini'],
            config_file_parser_class=configargparse.IniConfigParser(
                    ['global', 'vault', 'BAREOS']
                , split_ml_text_to_list=True
                ),
            description="Script console to vault volume for external storage."
        )
        parser.add_argument(
            "--config", help="full path to config file",
            is_config_file=True,
            default=f"/etc/bareos/{Path(__file__).stem}.ini"
        )
        # group = argparser.add_argument_group(title="Bareos Director connection options")
        # overriding parameters
        parser.add_argument(
            "--pools", help="comma separated list of pools to consider",
            dest="pools",type=str
        )
        parser.add_argument(
            "--autochanger", help="autochanger library storage name to use",
            dest="autochanger", type=str
        )
        parser.add_argument(
            "--max_age", help="how many days back to consider (default 21)",
            dest="max_age",default=21,type=int
        )
        parser.add_argument(
            "--min_age",
            help="how many days back to consider (default 1), 0 means all from the last minute",
            dest="min_age",default=1,type=int
        )
        parser.add_argument(
            "--return_time", help="how much days vaulting is expected (default 90)",
            dest="return_time",default=90,type=int
        )
        parser.add_argument(
            "--min_volgb", help="minimal volume gigabytes (default 4096GB, 4TB)",
            dest="min_volgb",default=4096,type=int
        )
        parser.add_argument(
            "--volume_status", help="comma separated list of volume status to consider",
            dest="volume_status",type=str
        )
        parser.add_argument(
            "--volstatus_to_archive", help="boolean to set volstatus to archive",
            dest="volstatus_to_archive",action="store_true"
        )
        parser.add_argument(
            "--update_slots_before", help="boolean to run update slots before",
            dest="update_slots_before",action="store_true"
        )
        parser.add_argument(
            "--export_slots", help="exp/imp slots range as 1-99",
            dest="export_slots", type=str
        )
        parser.add_argument(
            "--returndate_format",
            help="valid format string used by PostgreSQL to_char() default MMDDYYYY",
            dest="returndate_format", type=str,default="MMDDYYYY"
        )
        parser.add_argument(
            "--iron_acct",
            help="Iron Account Number, if not set it disable the export report generation",
            type=str,dest="iron_acct"
        )
        parser.add_argument(
            "--iron_name", help="Iron Name",type=str,dest="iron_name"
        )
        parser.add_argument(
            "--ftp_dir", help="directory where to place ftp list",type=str,
            dest="ftp_dir"
        )
        parser.add_argument(
            "--log_dir", help="directory where to place log file",type=str,
            dest="log_dir"
        )
        parser.add_argument(
            "--debug", action="store_true",
            default=False, help="enable debug logging"
        )
        parser.add_argument(
            "--verbose", action="store_true",
            default=False, help="enable verbose logging"
        )

        # """ Adding BAREOS default parameters
        #     [--name BAREOS_NAME]
        #     -p BAREOS_PASSWORD
        #     [--port BAREOS_PORT]
        #     [--address BAREOS_ADDRESS]
        #     [--timeout BAREOS_TIMEOUT]
        #     [--protocolversion BAREOS_PROTOCOLVERSION]
        #     [--pam-username BAREOS_PAM_USERNAME]
        #     [--pam-password BAREOS_PAM_PASSWORD]
        #     [--tls-psk-require]
        #     [--tls-version {v1,v1.1,v1.2}]
        # """
        bareos.bsock.DirectorConsoleJson.argparser_add_default_command_line_arguments(
            parser
        )

        try:
            self.args = parser.parse_args()
            logger.debug(self.args)
            with open(self.args.config,"r",encoding="utf-8") as f:
                logger.debug(f.read())
            self.bareos_args = (
                bareos.bsock.DirectorConsoleJson.
                    argparser_get_bareos_parameter(self.args)
                            )
        except bareos.exceptions.Error as berr:
            print(parser.format_values())
            print(f"Bareos Exception {str(berr)}")
            sys.exit(1)

        if self.args.verbose:
            logger.setLevel(logging.INFO)

        if self.args.debug:
            logger.setLevel(logging.DEBUG)

        logger.debug("args formatted: %s\n",parser.format_values())

#       logger.debug(logger.getLogger(me))
#       logger.debug(f"final args options: {str(self.args)}\n")
#       logger.debug(f"final bareos args options: {str(self.bareos_args)}\n")

        try:
            self._check_cli_args()
        except ValueError as type_err:
            logger.error("Value ERROR: %s\n",type_err, exc_info=True)
            return False
        except RuntimeError as error:
            logger.error(" -- Runtime unknown error -- %s", error, exc_info=True)
            return False

        logger.debug("leaving parse_cli_args()")
        return True


    def run(self):
        """ Main loop and entry point """
        logger.debug("entering run()")
        if not self.parse_cli_args():
            logger.error("ERROR: in parse_cli_args()\n", exc_info=True)
            return False
        logger.debug("slot update value %s\n",self.update_slots_before)
        logger.debug("volstatus archive value %s\n",self.volstatus_to_archive)
        try:
            self.dcj = bareos.bsock.DirectorConsoleJson(**self.bareos_args)
        except bareos.exceptions.Error as berr:
            msg = f"Bareos Exception {berr}"
            logger.error(msg, exc_info=True)
            print(msg)
            return False

        print("Bareos console connected\n")
        logger.debug("console connected\n")

        # Check bareos resources validity
        try:
            self._check_valid_storage()
            self._check_valid_pools()
        except TypeError as type_err:
            logger.error("Type error: %s\n",type_err, exc_info=True)
            return False
        except bareos.exceptions.Error as berr:
            logger.error("Bareos Exception %s\n", berr, exc_info=True)
            return False

        # Extract vault volumes to export
        try:
            if self.update_slots_before:
            # Update slots in case someone faulty reinsert one of the ejected volume
                self._bconsole_update_slots()

            self.vault_run_query()
            self.vault_parse_results()
        except bareos.exceptions.Error as berr:
            logger.error("Bareos Exception %s\n", berr, exc_info=True)
            return False
        except RuntimeError as error:
            logger.error(" -- Runtime unknown error -- %s\n",error, exc_info=True)
            return False

        if self.volumes_candidates:
            msg = f"{len(self.volumes_candidates)} candidate(s) volume(s) found\n"
            print(msg)
            msg_dbg = msg + f"Candidate(s) volume(s): {str(self.volumes_candidates)}\n"
            logger.debug(msg_dbg)
        else:
            msg = "No candidate volume to export found!\n"
            print(msg)
            logger.debug(msg)
            return True

        try:
            self.vault_create_eject_list()
            self.vault_eject_volumes()
        except bareos.exceptions.Error as berr:
            logger.error("Bareos Exception %s\n", berr, exc_info=True)
            return False
        except RuntimeError as error:
            logger.error(" -- Runtime unknown error -- %s\n",error, exc_info=True)
            return False

        logger.debug("Volumes exported: %s\n",str(self.volumes_exported))

        if self.volstatus_to_archive:
            logger.debug("Entering volstatus to 'Archive'\n")
            try:
                self._bconsole_set_volstatus()
                msg = "Volstatus updated to 'Archive'\n"
                print(msg)
                logger.debug(msg)
            except bareos.exceptions.Error as berr:
                logger.error("Bareos Exception %s\n", berr, exc_info=True)
                return False
            except RuntimeError as error:
                logger.error(" -- Runtime unknown error -- %s\n",error, exc_info=True)
                return False
        else:
            logger.debug("Volume status not updated to 'Archive'\n")

        logger.debug("End of run\n")
        return True


    def vault_run_query(self):
        """ Run vault query against normal console """
        logger.debug("entering vault_run_query()\n")

        s_p = "'"
        s_p += "','".join(self.pools)
        s_p += "'"

        max_age = f"date_trunc('day', now()) - interval '{self.max_age} days'"
        min_age = f"date_trunc('day', now()) - interval '{self.min_age} days'"
        # Select all tapes up to last minute if min_age is zero
        if self.min_age == 0:
            min_age = "date_trunc('minute', now())"

        vol_status = str(self.volume_status).replace('[','(').replace(']',')')

        sql = f"""
              select volumename, slot, storageid, volbytes, lastwritten, volretention,
                to_char (lastwritten + interval '{self.return_time} days',
                         '{self.returndate_format}') as returndate
              from media m
              where
               m.poolid in (select poolid from pool where name in ({s_p}))
              and
               m.storageid in (
                        select storageid from storage where name ~* '{self.autochanger}'
                    )
              and
               m.inchanger = 1
              and
               m.volbytes >= {self.min_volbytes}
              and
               m.lastwritten >= {max_age}
              and
               m.lastwritten < {min_age}
              and
               volstatus in {vol_status}
              order by m.lastwritten asc, m.volumename asc;
              """

        logger.debug("vault_query: '%s'\n",sql)

        try:
            self.sql_result = self.dcj.call(f".sql query=\"{sql}\"")["query"]
        except bareos.exceptions.JsonRpcErrorReceivedException as json_exp:
            logger.error("%s\n",json_exp, exc_info=True)
            raise bareos.exceptions.Error(json_exp)
        except bareos.exceptions.Error as berr:
            logger.error("%s\n",berr, exc_info=True)
            raise berr

        logger.debug("vault_sql_result: %s\n",self.sql_result)


    def vault_parse_results(self):
        """ Parse sql_result and format to export vault volume list """
        logger.debug("entering vault_parse_results()")
        if len(self.sql_result) <= 0:
            logger.debug("No candidates volumes found\n")
            return
        for volume in self.sql_result:
            self.volumes_candidates.append(
               {
                "volumename": volume["volumename"],
                "slotid": volume["slot"],
                "storageid": volume["storageid"],
                "volbytes": volume["volbytes"],
                "lastwritten": volume["lastwritten"],
                "volretention": volume["volretention"],
                "returndate": volume["returndate"]
                }
            )

        logger.debug("Candidates Volumes: %s\n", str(self.volumes_candidates))
        logger.debug("leaving vault_parse_results()\n")


    def vault_create_eject_list(self):
        """ Create the list of vaults volumes that are candidate for eject.
            We will print that list of candidates. When the list is empty,
            operators knows that no more tape have to be ejected.
        """
        logger.debug("entering vault_create_eject_list()\n")

        export_list = StringIO()

        # we want a better visual formatting
        # f"{left_alignment : <20}{center_alignment : ^15}{right_alignment : >20}"

        # Write content to the eject.list
        try:
            header= ( "# "
                f"{'VolumeName' : ^12}  "
                f"{'Slot' : ^6}   "
                f"{'StorageId' : ^9}  "
                f"{'VolBytes' : ^18}   "
                f"{'LastWritten' : ^20}   "
                f"{'VolRetention' : ^12}   "
                f"{'ReturnDate' : ^10}\n"
                )
            export_list.write(header)
            for vol in self.volumes_candidates:
                eject_line = (
                    f"{vol['volumename'] : <14}  "
                    f"{vol['slotid'] : >6}   "
                    f"{vol['storageid'] : >9}  "
                    f"{vol['volbytes'] : >18}   "
                    f"{vol['lastwritten'] : <20}   "
                    f"{vol['volretention'] : >12}   "
                    f"{vol['returndate'] : <10}\n"
                    )
                export_list.write(eject_line)

            print(export_list.getvalue())
            export_list.close()
        except OSError as os_error :
            msg_err = f"Fatal OS ERROR can't write \"export_list\" {os_error}\n"
            logger.error(msg_err, exc_info=True)
            print(msg_err)
            raise
        except Exception as err:
            msg_err = f"Unknown Error occurred {err}\n"
            logger.error(msg_err, exc_info=True)
            print(msg_err)
            raise

        logger.debug("leaving vault_create_eject_list()\n")

    def vault_eject_volumes(self):
        """ Export the volumes by using the autochanger's import/export slots
            write exported volumes and return date to the ftp file to be transmitted.
        """
        logger.debug("entering vault_eject_volumes()\n")
        # https://docs.bareos.org/TasksAndConcepts/BareosConsole.html#console-commands
        # export section
        # export storage=<storage-name> srcslots=<slot-selection>
        # [dstslots=<slot-selection> volume=<volume-name> scan]

        count = 0
        size_s = 0
        size_b = 0
        size_f = 0
        f_stats = None

        if self.iron_acct:
            # Normalized ftp filename
            self.ftp_file = os.path.join(self.ftp_dir,
                                (
                                f"{self.iron_acct}_{self.iron_name}{self.autochanger}_"
                                f"{self.date_time}_V.IM"
                                )
                            )

            ftp_file_header_d = (
                    f"StartHeaderText~D~{self.iron_acct}~{self.iron_name}"
                    f"-{self.autochanger}~EndHeaderText"
                )
            ftp_file_footer_d = (
                    f"StartFooterText~D~{self.iron_acct}~{self.iron_name}"
                    f"-{self.autochanger}-{count}~EndFooterText"
                )

            logger.debug("Creating ftp file: %s\n",self.ftp_file)

            # We create destination directory if not existent
            try:
                if not os.path.isdir(self.ftp_dir):
                    os.makedirs(self.ftp_dir)
                    msg = f"OS creating ftp dir {self.ftp_dir}"
                    logger.debug(msg)
                    print(msg)
            except OSError as os_error :
                msg = f"Fatal ERROR while makedirs('{self.ftp_dir}') \"{os_error}\"\n"
                logger.error(msg, exc_info=True)
                raise
            except Exception as err:
                logger.error("Unknown error %s\n",err, exc_info=True)
                raise

        # Run export command
        try:
            self._bconsole_export_volumes()
        except bareos.exceptions.Error as berr:
            msg = f"bconsole export volume failure {berr}\n"
            logger.error(msg, exc_info=True)
            raise
        except Exception as err:
            logger.error("Unknown error during export of volumes %s",err, exc_info=True)
            raise

        if self.iron_acct:
            logging.debug("Writing ftp file %s\n",self.ftp_file)
            # Write content to the ftp_file
            try:
                s = StringIO()
                s.write(f"{ftp_file_header_d}\n")
                for vol in self.volumes_exported:
                    ftp_line = (
                            f"{vol['volumename']}"
                                "    "
                            f"{vol['returndate']}\n"
                            )
                    s.write(ftp_line)
                s.write(f"{ftp_file_footer_d}\n")
                size_s = len(s.getvalue())
                logging.debug("%s\n",s.getvalue())

                with open(self.ftp_file, "w", encoding="utf-8") as f:
                    size_f = f.write(s.getvalue())
                    f.close()

                if size_f != size_s:
                    msg = f"Written size({size_f}) not equal to data length ({size_s})"
                    print(msg)
                    raise ValueError(msg)

            except OSError as os_error :
                msg = f"Fatal ERROR can't write \"{self.ftp_file}\" {os_error}\n"
                logger.error(msg, exc_info=True)
                raise
            except ValueError as val_error :
                msg = f"Fatal ERROR occur with \"{self.ftp_file}\" {val_error}\n"
                logger.error(msg, exc_info=True)
                raise
            except Exception as err:
                msg = f"Unknown error {err}\n"
                logger.error(msg, exc_info=True)
                raise

            msg= f"Created ftp file: {self.ftp_file}\n"
            print(msg)
            logger.debug(msg)
            # Force Reading file again to certify content
            try:
                f_stats = os.stat(self.ftp_file)
                with open(self.ftp_file, "r", encoding="utf-8") as f:
                    buf = f.read()
                    f.close()
                size_b = len(buf)
                if size_s != size_b:
                    msg = f"Read size({size_b}) not equal to data length ({size_s})\n"
                    print (msg)
                    logger.error(msg)
                    raise ValueError(msg)
                sha256_b = hashlib.sha256(buf.encode()).hexdigest()
                sha256_s = hashlib.sha256(s.getvalue().encode()).hexdigest()
                if sha256_b != sha256_s:
                    msg = {
                        f"Error sha256 checksum differs between file({sha256_b}) "
                        f"and expected ({sha256_s})"
                    }
                    print (msg)
                    logger.error(msg)
                    raise ValueError(msg)

                ctime = datetime.datetime.fromtimestamp(
                    f_stats.st_ctime,datetime.UTC).strftime('%Y-%m-%dT%H:%M:%S.%f%z')
                atime = datetime.datetime.fromtimestamp(
                    f_stats.st_atime,datetime.UTC).strftime('%Y-%m-%dT%H:%M:%S.%f%z')
                mtime = datetime.datetime.fromtimestamp(
                    f_stats.st_mtime,datetime.UTC).strftime('%Y-%m-%dT%H:%M:%S.%f%z')
                t_stat = (
                            f"{'----------' : <10} {' ' : <15}\n"
                            f"{'os stats ' : <10}{self.ftp_file : <15}\n"
                            f"{'----------' : <10}\n"
                            f"{'st_mode ' : <10}{f_stats.st_mode : <15}\n"
                            f"{'st_ino '  : <10}{f_stats.st_ino : <15}\n"
                            f"{'st_dev '  : <10}{f_stats.st_dev : <15}\n"
                            f"{'st_nlink ': <10}{f_stats.st_nlink : <15}\n"
                            f"{'st_uid'   : <10}{f_stats.st_uid : <15}\n"
                            f"{'st_gid'   : <10}{f_stats.st_gid : <15}\n"
                            f"{'st_size'  : <10}{f_stats.st_size : <15}\n"
                            f"{'st_atime' : <10}{atime : <15}\n"
                            f"{'st_mtime' : <10}{mtime : <15}\n"
                            f"{'st_ctime' : <10}{ctime : <15}\n"
                            f"{'----------' : <10}\n"
                            f"{'sha256sum' : <10}{sha256_b : <15}\n"
                            f"{'----------' : <10}\n"
                        )
                s_value = s.getvalue()
                print(s_value)
                print(t_stat)
                logger.debug(
                    (
                        s_value,
                        "\n",
                        t_stat
                    )
                )

            except OSError as os_error :
                msg = f"Fatal OS_ERROR can't write \"{self.ftp_file}\" {os_error}\n"
                print(msg)
                raise
            except ValueError as val_error :
                msg = f"Fatal ERROR occur with \"{self.ftp_file}\" {val_error}\n"
                print(msg)
                raise
            except Exception as err:
                print(err)
                raise

        logger.debug("leaving vault_eject_volumes()\n")


if __name__ == "__main__":

    sys.exit(not MediaVault().run())


# vim: tabstop=4 expandtab shiftwidth=4 softtabstop=4
