#!/usr/bin/env python
# -*- coding: utf-8 -*-

# Bareos python class PoC option plugin to index files
# with Tika and Elasticsearch, including Bareos JobId
#
# Configure your Elasticsearch server and Tika-Jar
# inline below
#
# This is Proof of Concept code. Use with caution
# TODO: 
# - Make Tika and Elastic configurable by file / option
# - Better error handling
# 
# (c) 2019 Bareos GmbH & Co. KG
# AGPL v.3
# Author: Maik Aussendorf

from bareosfd import *
from bareos_fd_consts import *
import os
from  BareosFdPluginBaseclass import *
import BareosFdWrapper
from elasticsearch import Elasticsearch
from tikapp import TikaApp
import subprocess
import sys
import json


class BareosFdPluginFileElasticsearch  (BareosFdPluginBaseclass):

    def handle_backup_file(self,context, savepkt):
        DebugMessage(context, 100, "handle_backup_file called with " + str(savepkt) + "\n");
        DebugMessage(context, 100, "fname: " + savepkt.fname + " Type: " + str(savepkt.type) + "\n");
        if ( savepkt.type == bFileType['FT_REG'] ):
            DebugMessage(context, 100, "regulaer file, do something now...\n");
            # configure your Elasticsearch server here:
            es = Elasticsearch([{'host': '192.168.17.2', 'port': 9200}])
            # configure your TikaApp jar file here:
            try:
                tika_client = TikaApp(file_jar="/usr/local/bin/tika-app-1.20.jar")
            except Exception as ex:
                JobMessage(context,  bJobMessageType['M_ERROR'], 'Error indexing %s. Tika error: %s' % (savepkt.fname, str(ex)))
                return bRCs['bRC_OK'];
            # tika_client has several parser options
            # Next one is for metadata only:
            #result_payload=tika_client.extract_only_metadata(savepkt.fname)
            # This one includes file contents as text:
            result_payload=tika_client.extract_all_content(savepkt.fname)
            # result_payload is a list of json-strings. Nested structes like
            # tar-files or emails with attachments or inline documents are
            # returned as distinct json string.
            # The first string [0] contains information for the main file
            # TODO: care about nested structures, for now we only takte the first/main file 
            data = json.loads(result_payload)[0]
            # Tika adds some emptylines at the beginning of content, we strip it here
            if 'X-TIKA:content' in data:
                data['X-TIKA:content'] = data['X-TIKA:content'].strip()
            data['bareos_jobId'] = self.jobId
            data['bareos_fdname'] = self.fdname
            data['bareos_joblevel'] = unichr(self.level)
            data['bareos_directory'] = os.path.dirname(savepkt.fname) 
            try:
                esRes = es.index (index="bareos-test", doc_type='_doc', body=data)
            except Exception as ex:
                JobMessage(context,  bJobMessageType['M_ERROR'], 'Error indexing %s. Elastic error: %s' % (savepkt.fname, str(ex)))
        return bRCs['bRC_OK'];
