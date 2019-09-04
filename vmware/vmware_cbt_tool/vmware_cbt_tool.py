#!/usr/bin/env python2
# -*- coding: utf-8 -*-
# BAREOS® - Backup Archiving REcovery Open Sourced
#
# Copyright (C) 2014-2014 Bareos GmbH & Co. KG
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
#
# Author: Stephan Duehr

"""
Python program for enabling/disabling/resetting CBT on a VMware VM
"""

from pyVim.connect import SmartConnect, Disconnect
from pyVmomi import vim, vmodl

import argparse
import atexit
import getpass
import sys
import os.path

from collections import defaultdict


def GetArgs():
    """
    Supports the command-line arguments listed below.
    """

    parser = argparse.ArgumentParser(
        description='Process args for enabling/disabling/resetting CBT')
    parser.add_argument('-s', '--host',
                        required=True,
                        action='store',
                        help='Remote host to connect to')
    parser.add_argument('-o', '--port',
                        type=int,
                        default=443,
                        action='store',
                        help='Port to connect on')
    parser.add_argument('-u', '--user',
                        required=True,
                        action='store',
                        help='User name to use when connecting to host')
    parser.add_argument('-p', '--password',
                        required=False,
                        action='store',
                        help='Password to use when connecting to host')
    parser.add_argument('-d', '--datacenter',
                        required=True,
                        action='store',
                        help='DataCenter Name')
    parser.add_argument('-f', '--folder',
                        required=False,
                        action='store',
                        help='Folder Name (must start with /, use / for root folder')
    parser.add_argument('-v', '--vmname',
                        required=False,
                        action='append',
                        help='Names of the Virtual Machines')
    parser.add_argument('--vm-uuid',
                        required=False,
                        action='append',
                        help='Instance UUIDs of the Virtual Machines')
    parser.add_argument('--enablecbt',
                        action='store_true',
                        default=False,
                        help='Enable CBT')
    parser.add_argument('--disablecbt',
                        action='store_true',
                        default=False,
                        help='Disable CBT')
    parser.add_argument('--resetcbt',
                        action='store_true',
                        default=False,
                        help='Reset CBT (disable, then enable)')
    parser.add_argument('--info',
                        action='store_true',
                        default=False,
                        help='Show information (CBT supported and enabled or disabled)')
    parser.add_argument('--listall',
                        action='store_true',
                        default=False,
                        help='List all VMs in the given datacenter with UUID and containing folder')
    args = parser.parse_args()

    if [args.enablecbt, args.disablecbt, args.resetcbt, args.info, args.listall].count(True) > 1:
        parser.error("Only one of --enablecbt, --disablecbt, --resetcbt, --info, --listall allowed")

    if args.folder and not args.folder.startswith('/'):
        parser.error("Folder name must start with /")

    return args


def main():
    """
    Python program for enabling/disabling/resetting CBT on a VMware VM
    """

    # newer Python versions, eg. Debian 8/Python >= 2.7.9 and
    # CentOS/RHEL since 7.4 by default do SSL cert verification,
    # we then try to disable it here.
    # see https://github.com/vmware/pyvmomi/issues/212
    py_ver = sys.version_info[0:3]
    if py_ver[0] == 2 and py_ver[1] == 7 and py_ver[2] >= 5:
        import ssl
        try:
            ssl._create_default_https_context = ssl._create_unverified_context
        except AttributeError:
            pass

    args = GetArgs()
    if args.password:
        password = args.password
    else:
        password = getpass.getpass(
            prompt='Enter password for host %s and user %s: ' %
            (args.host, args.user))

    try:

        si = None
        try:
            si = SmartConnect(host=args.host,
                              user=args.user,
                              pwd=password,
                              port=int(args.port))
        except IOError as e:
            pass
        if not si:
            print ("Cannot connect to specified host using specified"
                   "username and password")
            sys.exit()

        atexit.register(Disconnect, si)

        content = si.content

        dcftree = {}
        dcView = content.viewManager.CreateContainerView(content.rootFolder,
                                                         [vim.Datacenter],
                                                         False)
        dcList = dcView.view
        dcView.Destroy()
        for dc in dcList:
            if dc.name == args.datacenter:
                dcftree[dc.name] = {}
                folder = ''
                get_dcftree(dcftree[dc.name], folder, dc.vmFolder)

        if args.listall:
            print_dcftree(dcftree)
            sys.exit(0)

        vm = None

        for vm in get_vm_list(args, dcftree):

            print "INFO: VM %s CBT supported: %s" % (
                vm.name, vm.capability.changeTrackingSupported)
            print "INFO: VM %s CBT enabled: %s" % (
                vm.name, vm.config.changeTrackingEnabled)

            if args.enablecbt:
                print "INFO: VM %s trying to enable CBT now" % (vm.name)
                enable_cbt(si, vm)
            if args.disablecbt:
                print "INFO: VM %s trying to disable CBT now" % (vm.name)
                disable_cbt(si, vm)
            if args.resetcbt:
                print "INFO: VM %s trying to reset CBT now" % (vm.name)
                disable_cbt(si, vm)
                enable_cbt(si, vm)

    except vmodl.MethodFault as e:
        print "Caught vmodl fault : " + e.msg
    except Exception as e:
        print "Caught unexpected Exception : " + str(e)
        raise


def get_vm_list(args, dcftree):
    """
    Check if VMs are specified by UUID or folder + VM names and
    return list of VMs to work with
    """
    vm_list = []
    if args.vmname:
        for vmname in args.vmname:
            folder_u = unicode(args.folder, 'utf8')
            vmname_u = unicode(vmname, 'utf8')
            vm_path = folder_u + '/' + vmname_u
            if args.folder.endswith('/'):
                vm_path = folder_u + vmname_u

            if args.datacenter not in dcftree:
                print "ERROR: Could not find datacenter %s" % (args.datacenter)
                sys.exit(1)

            if vm_path not in dcftree[args.datacenter]:
                print "ERROR: Could not find VM %s in folder %s" % (
                    vmname_u, folder_u)
                sys.exit(1)

            vm_list.append(dcftree[args.datacenter][vm_path])

    elif args.vm_uuid:
        vms_by_uuid = get_vms_by_uuid(dcftree)
        for vm_uuid in args.vm_uuid:
            if vm_uuid not in vms_by_uuid:
                print "ERROR: Could not find VM with instance UUID %s" % (vm_uuid)
                sys.exit(1)

            vm_list.append(vms_by_uuid[vm_uuid])

    else:
        print "ERROR: No VMs given, neither by folder + name nor by UUID"
        sys.exit(1)

    return vm_list


def enable_cbt(si, vm):
    if not vm.capability.changeTrackingSupported:
        print "ERROR: VM %s does not support CBT" % (vm.name)
        return False

    if vm.config.changeTrackingEnabled:
        print "INFO: VM %s is already CBT enabled" % (vm.name)
        return True

    cspec = vim.vm.ConfigSpec()
    cspec.changeTrackingEnabled = True
    task = vm.ReconfigVM_Task(cspec)
    WaitForTasks([task], si)
    return create_and_remove_snapshot(si, vm)


def disable_cbt(si, vm):
    if not vm.capability.changeTrackingSupported:
        print "ERROR: VM %s does not support CBT" % (vm.name)
        return False

    if not vm.config.changeTrackingEnabled:
        print "INFO: VM %s is already CBT disabled" % (vm.name)
        return True

    cspec = vim.vm.ConfigSpec()
    cspec.changeTrackingEnabled = False
    task = vm.ReconfigVM_Task(cspec)
    WaitForTasks([task], si)
    return create_and_remove_snapshot(si, vm)


def get_dcftree(dcf, folder, vm_folder):
    for vm_or_folder in vm_folder.childEntity:
        if isinstance(vm_or_folder, vim.VirtualMachine):
            dcf[folder + '/' + vm_or_folder.name] = vm_or_folder
        elif isinstance(vm_or_folder, vim.Folder):
            get_dcftree(dcf, folder + '/' + vm_or_folder.name, vm_or_folder)
        elif isinstance(vm_or_folder, vim.VirtualApp):
            # vm_or_folder is a vApp in this case, contains a list a VMs
            for vapp_vm in vm_or_folder.vm:
                dcf[folder + '/' + vm_or_folder.name + '/' + vapp_vm.name] = vapp_vm
        else:
            print "NOTE: %s is neither Folder nor VirtualMachine nor vApp, ignoring." % vm_or_folder


def create_vm_snapshot(si, vm):
    """
    creates a snapshot on the given vm
    """
    create_snap_task = None
    create_snap_result = None
    try:
        create_snap_task = vm.CreateSnapshot_Task(
            name='CBTtoolTmpSnap',
            description='CBT tool temporary snapshot',
            memory=False,
            quiesce=False)
    except vmodl.MethodFault as e:
        print "Failed to create snapshot %s" % (e.msg)
        return False

    WaitForTasks([create_snap_task], si)
    create_snap_result = create_snap_task.info.result
    return create_snap_result


def remove_vm_snapshot(si, create_snap_result):
    """
    removes a given snapshot
    """
    remove_snap_task = None
    try:
        remove_snap_task = create_snap_result.RemoveSnapshot_Task(
            removeChildren=True)
    except vmodl.MethodFault as e:
        print "Failed to remove snapshot %s" % (e.msg)
        return False

    WaitForTasks([remove_snap_task], si)
    return True


def create_and_remove_snapshot(si, vm):
    """
    creates, then removes a snapshot,
    also named stun-unstun cycle
    """
    print "INFO: VM %s trying to create and remove a snapshot to activate CBT" % (vm.name)
    snapshot_result = create_vm_snapshot(si, vm)
    if snapshot_result:
        if remove_vm_snapshot(si, snapshot_result):
            print "INFO: VM %s successfully created and removed snapshot" % (vm.name)
            return True

    return False


def WaitForTasks(tasks, si):
    """
    Given the service instance si and tasks, it returns after all the
    tasks are complete
    """

    pc = si.content.propertyCollector

    taskList = [str(task) for task in tasks]

    # Create filter
    objSpecs = [vmodl.query.PropertyCollector.ObjectSpec(obj=task)
                for task in tasks]
    propSpec = vmodl.query.PropertyCollector.PropertySpec(type=vim.Task,
                                                          pathSet=[], all=True)
    filterSpec = vmodl.query.PropertyCollector.FilterSpec()
    filterSpec.objectSet = objSpecs
    filterSpec.propSet = [propSpec]
    filter = pc.CreateFilter(filterSpec, True)

    try:
        version, state = None, None

        # Loop looking for updates till the state moves to a completed state.
        while len(taskList):
            update = pc.WaitForUpdates(version)
            for filterSet in update.filterSet:
                for objSet in filterSet.objectSet:
                    task = objSet.obj
                    for change in objSet.changeSet:
                        if change.name == 'info':
                            state = change.val.state
                        elif change.name == 'info.state':
                            state = change.val
                        else:
                            continue

                        if not str(task) in taskList:
                            continue

                        if state == vim.TaskInfo.State.success:
                            # Remove task from taskList
                            taskList.remove(str(task))
                        elif state == vim.TaskInfo.State.error:
                            raise task.info.error
            # Move to next version
            version = update.version
    finally:
        if filter:
            filter.Destroy()

    return True


def print_dcftree(dcftree):
    """
    Print the Datacenter Folder Tree
    """
    for dc in dcftree:
        dcftree_by_folder = defaultdict(dict)
        for vm_path in dcftree[dc]:
            dcftree_by_folder[os.path.dirname(vm_path)][os.path.basename(vm_path)] = \
                dcftree[dc][vm_path].config.instanceUuid
        print "DataCenter: %s" % dc
        print "%-36s %-50s %s" % ('VM-Instance-UUID', 'VM-Name', 'VM-Folder and/or vApp')
        for vm_folder in sorted(dcftree_by_folder):
            for vm_name in sorted(dcftree_by_folder[vm_folder]):
                print "%36s %-50s %s" % (dcftree_by_folder[vm_folder][vm_name], vm_name, vm_folder)


def get_vms_by_uuid(dcftree):
    """
    Get the VMs in the Datacenter as dict with UUID as key
    """
    vms_by_uuid = {}
    for dc in dcftree:
        for vm_path in dcftree[dc]:
            vms_by_uuid[dcftree[dc][vm_path].config.instanceUuid] = dcftree[dc][vm_path]

    return vms_by_uuid


# Start program
if __name__ == "__main__":
    main()

# vim: tabstop=4 expandtab shiftwidth=4 softtabstop=4
