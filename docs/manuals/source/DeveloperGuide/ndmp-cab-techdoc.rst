.. _section-ndmp-cab-techdoc:

NetApp NDMP CAB
===============

Scope
-----

This document summarizes the current findings about NetApp ONTAP NDMP
Cluster Aware Backup (CAB) and how it fits into the existing Bareos NDMP
implementation.

The focus is intentionally technical:

* how Bareos currently performs NDMP backups and restores,
* what CAB changes in the NDMP message flow,
* which protocol details could be confirmed publicly, and
* which details are still missing.

Current Bareos NDMP architecture
--------------------------------

Bareos already implements NDMP in a dedicated code path. NDMP jobs are
not normal File Daemon plugin jobs.

The current architecture is:

Data Management Agent (DMA)
   the Bareos Director

Data Agent
   the NDMP server on the NAS system

Tape Agent
   either the Bareos Storage Daemon in ``NDMP_BAREOS`` mode or an
   external NDMP tape agent in ``NDMP_NATIVE`` mode

The relevant code is split between the Director-side NDMP job handling
and the shared NDMP transport layer:

* ``core/src/dird/ndmp_dma_backup_*.cc``
* ``core/src/dird/ndmp_dma_restore_*.cc``
* ``core/src/ndmp/``

Within ``core/src/ndmp/``, the current startup flow is centered around:

* ``ndmca_connect_control_agent()``
* ``ndmca_backreco_startup()``
* ``ndmca_data_listen()``
* ``ndmca_mover_listen()``
* ``ndmca_data_connect()``
* ``ndmca_mover_connect()``
* ``ndmca_data_start_backup()``
* ``ndmca_data_start_recover()``

Today, Bareos learns the later data or mover endpoint from the normal
NDMP listen/connect exchange. There is no separate generic
filesystem-to-node resolution step in front of the protocol.

What CAB is
-----------

NetApp CAB is an NDMPv4 extension used by ONTAP in clustered
environments. Its purpose is not to add a second management protocol,
but to extend the NDMP control flow so that the NDMP server can resolve
the correct data-serving node and logical interface for the volume that
is being backed up or restored.

The important practical consequence is that the DMA must tell the NDMP
server which filesystem or volume is involved before the normal data
connection setup is completed.

CAB therefore belongs inside the NDMP message flow. It should not be
modeled as an external resolver service and not as a Bareos File Daemon
plugin feature.

Confirmed CAB protocol details
------------------------------

The following points could be confirmed from NetApp ONTAP documentation
and from public source code that contains NetApp-derived NDMP extension
definitions.

Extension negotiation
~~~~~~~~~~~~~~~~~~~~~

CAB is negotiated through the standard NDMPv4 extension mechanisms:

* ``NDMP_CONFIG_GET_EXT_LIST``
* ``NDMP_CONFIG_SET_EXT_LIST``

The confirmed CAB extension identifiers are:

* extension class id: ``0x2052``
* extension version: ``1``

CAB message id
~~~~~~~~~~~~~~

The CAB prepare message is:

* ``NDMP_CAB_DATA_CONN_PREPARE = 0x20520001``

Position in the message flow
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The CAB prepare message is sent after authentication and extension
negotiation, but before the normal data connection setup.

The resulting control flow is:

1. ``NDMP_CONNECT_OPEN``
2. ``NDMP_CONFIG_GET_AUTH_ATTR`` when required by the authentication mode
3. ``NDMP_CONNECT_CLIENT_AUTH``
4. ``NDMP_CONFIG_GET_EXT_LIST``
5. ``NDMP_CONFIG_SET_EXT_LIST``
6. ``NDMP_CAB_DATA_CONN_PREPARE``
7. ``NDMP_DATA_LISTEN`` or ``NDMP_DATA_CONNECT``
8. ``NDMP_MOVER_LISTEN`` or ``NDMP_MOVER_CONNECT`` when the topology
   requires it
9. ``NDMP_DATA_START_BACKUP`` or ``NDMP_DATA_START_RECOVER``

The important point is that CAB is a preparation phase for the later
listen/connect exchange. It changes how ONTAP selects the addresses used
in those later messages.

CAB request body
~~~~~~~~~~~~~~~~

The public implementation that was found does not use a new rich
request-body type for CAB. Instead, it serializes a request with the
same layout as ``ndmp_data_start_backup_request_v4``:

* ``bu_type``
* ``env[]``

The crucial environment information is the target filesystem or volume.
In practice, the request contains the same kind of information that
Bareos already constructs for NDMP backup startup, including an entry
such as ``FILESYSTEM=<path-or-volume>``.

CAB reply body
~~~~~~~~~~~~~~

The CAB reply body only contains an NDMP error code. It does not, by
itself, return a dedicated node name, LIF, or explicit affinity object.

Special CAB header
~~~~~~~~~~~~~~~~~~

The public implementation uses a special header for
``NDMP_CAB_DATA_CONN_PREPARE`` instead of the normal NDMP request
header.

That header has the normal NDMP header fields plus one additional
``length`` field. In the public implementation that field is hardcoded
to ``1``.

This means a Bareos CAB implementation likely needs explicit support for
the CAB message header shape in the NDMP core rather than only adding a
new message id to an existing dispatch table.

Where the address or affinity information appears
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The best public evidence indicates that CAB does not return the desired
endpoint directly in the CAB reply. Instead, the result of the prepare
step shows up in the later NDMPv4 address objects used by the normal
listen/connect messages.

The relevant NDMPv4 structures are:

* ``ndmp_tcp_addr_v4.addr_env[]``
* ``ndmp_tcp_ipv6_addr_v4.addr_env[]``

These address structures are embedded in replies such as:

* ``NDMP_DATA_LISTEN``
* ``NDMP_MOVER_LISTEN``

Therefore, CAB should be understood as:

1. tell ONTAP which filesystem or volume is involved,
2. let ONTAP resolve the owning node and usable interface,
3. obtain the resulting address and affinity data from the normal NDMP
   listen/connect path.

Address-type consistency
~~~~~~~~~~~~~~~~~~~~~~~~

NetApp documents explicit precondition errors when
``NDMP_CAB_DATA_CONN_PREPARE`` and the following ``NDMP_DATA_LISTEN`` or
``NDMP_DATA_CONNECT`` do not use matching address families or address
types.

This is an important implementation constraint: CAB prepare cannot be
handled as a generic side call with different connection assumptions
than the later transport setup.

How this maps onto Bareos
-------------------------

Current Bareos NDMP control flow already has a natural place for CAB:
inside ``core/src/ndmp/`` before the normal data or mover listen/connect
phase starts.

The main consequences are:

1. Bareos needs NDMPv4 extension negotiation support for CAB.
2. Bareos needs a new ``NDMP_CAB_DATA_CONN_PREPARE`` request path.
3. That request path needs support for the special CAB header.
4. The startup flow must invoke CAB prepare before
   ``ndmca_data_listen()`` or ``ndmca_data_connect()``.
5. The later listen/connect replies should preserve and expose
   ``addr_env[]`` so that location or affinity metadata is not lost.

The most likely integration points are:

* ``ndmca_connect_control_agent()`` for extension discovery and
  negotiation,
* ``ndmca_backreco_startup()`` for the CAB prepare phase,
* ``ndmca_data_listen()`` and ``ndmca_mover_listen()`` for consuming the
  returned NDMPv4 address structures,
* the Director-side NDMP environment builders for providing the
  filesystem or volume identity in the prepare request.

In other words, the right implementation model is not:

* resolve filesystem to node outside NDMP,
* replace the data agent address in configuration,
* continue with unchanged NDMP flow.

Instead, the right model is:

* negotiate the CAB extension,
* send the in-band prepare message for the current filesystem or volume,
* continue with the normal NDMP data connection flow that now reflects
  the CAB decision.

What is still unknown
---------------------

One important detail is still missing from the public information that
was found: the exact vendor-specific key names used in ``addr_env[]`` by
ONTAP to describe node, LIF, affinity, or locality.

The structure location is known, but the concrete string keys and their
exact semantics were not found in public documentation.

Without those keys, Bareos can still implement the CAB message flow, but
it cannot yet interpret all returned affinity metadata in a
vendor-specific way.

The most reliable ways to close this gap are:

* a packet capture of a real ONTAP CAB session,
* a fuller NetApp CAB specification,
* or vendor SDK material that documents the ``addr_env[]`` entries.

Practical conclusion
--------------------

The main result of the investigation is that NetApp CAB is not a
standalone lookup service. It is an NDMPv4 extension that modifies the
normal NDMP connection setup.

For Bareos this means:

* the work belongs in ``core/src/ndmp/``,
* the prepare step must be inserted into the NDMP startup sequence,
* the existing Director-side NDMP environment data can likely provide
  the required filesystem identity,
* and the later NDMPv4 address objects must be preserved because that is
  where CAB-related address and affinity metadata becomes visible.

References
----------

The following public sources were used during the investigation:

* NetApp ONTAP documentation on Cluster Aware Backup
* NetApp ONTAP documentation on NDMP data connection types
* NetApp ONTAP documentation on CAB prepare precondition errors for
  ``DATA_LISTEN`` and ``DATA_CONNECT``
* Bareos sources in ``core/src/dird/`` and ``core/src/ndmp/``
* public ``open-eBackup`` sources containing NetApp-derived NDMP
  extension definitions and CAB handling logic
