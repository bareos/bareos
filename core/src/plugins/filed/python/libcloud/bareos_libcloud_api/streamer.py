#!/usr/bin/env python
# -*- coding: utf-8 -*-
# Copyright (C) 2020-2025 Bareos GmbH & Co. KG
#
# This program is Free Software; you can redistribute it and/or
# modify it under the terms of version three of the GNU Affero General Public
# License as published by the Free Software Foundation, which is
# listed in the file LICENSE.
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

import os
import time

from bareos_libcloud_api.process_base import ThreadType
from bareos_libcloud_api.process_base import ProcessBase


class Streamer(ProcessBase):
    """
    Class for streaming download of objects to a pipe.

    Attributes:
        obj (libcloud.storage.base.Object): Configuration options.
        chunk_size (int): Chunk Size to use for downloading.
    """

    def __init__(
        self,
        options,
        obj,
        pipe_write_fd,
        message_queue,
        chunk_size=5242880,
    ):
        """
        Initialize Streamer.

        Args:
            obj (libcloud.storage.base.Object): Configuration options.
            pipe_write_fd (int): Pipe write file descriptor.
            message_queue (Queue): Queue for messages.
            chunk_size (int): Chunk Size to use for downloading, default 5242880 (5M).
        """
        super().__init__(1, ThreadType.STREAMER, message_queue)
        self.options = options
        self.obj = obj
        self.pipe_write_fd = pipe_write_fd
        self.chunk_size = chunk_size

    def run_process(self):
        """
        Stream download of an object to a pipe.
        """
        # Note: Any message here will only be put in the message queue
        # which is only processed when the next file backup starts.
        # Directly using bareosfd.
        start_time = time.time()
        self.debug_message(
            100, f'Start streaming "{self.obj.name}" at {start_time:.4f}'
        )
        w_file = os.fdopen(self.pipe_write_fd, "wb", buffering=0)

        try:
            # default libcloud chunk size seems to be 5M
            for chunk in self.obj.as_stream(chunk_size=self.chunk_size):
                w_file.write(chunk)
        except Exception as err:
            self.error_message(
                f'Error writing chunks to pipe for "{self.obj.name}": {err}'
            )
            # ensure write-end is closed on errors so reader can detect EOF
            try:
                w_file.close()
            except Exception as e:
                self.error_message(f'Error closing pipe for "{self.obj.name}": {e}')

            if self.options.get("fail_on_download_error"):
                self.abort_message()

        finally:
            # always close the writer to signal EOF to the reader
            w_file.close()

        end_time = time.time()
        elapsed_time = end_time - start_time
        throughput = self.obj.size / (1024 * 1024 * elapsed_time)
        self.debug_message(
            100,
            f'Stop streaming "{self.obj.name}" at {end_time:.4f} '
            f"duration {elapsed_time:.4f} seconds "
            f"throughput {throughput:.2f} MB/s",
        )
        self.shutdown_event.set()
