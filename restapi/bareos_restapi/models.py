#   BAREOS - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2020-2023 Bareos GmbH & Co. KG
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

from pydantic import BaseModel, Field, PositiveInt
from enum import Enum
import pathlib
from typing import Optional, List
from typing_extensions import Annotated
from fastapi import Depends, FastAPI, HTTPException, status, Response, Path, Body, Query


class Token(BaseModel):
    access_token: str
    token_type: str


class TokenData(BaseModel):
    username: Optional[str] = None


class User(BaseModel):
    username: str
    directorName: Optional[str] = None


class UserInDB(User):
    hashed_password: str


class bareosBool(str, Enum):
    yes = "yes"
    no = "no"

    def __str__(self):
        return self.name

    def __bool__(self):
        return self.name == "yes"


class bareosFlag(str, Enum):
    """
    Same as bareosBool but with different internal handling
    """

    yes = "yes"
    no = "no"

    def __str__(self):
        return self.name

    def __bool__(self):
        return self.name == "yes"


class bareosReplaceOption(str, Enum):
    """
    Replace option used by restore command
    """

    always = "always"
    never = "never"
    ifolder = "ifolder"
    ifnewer = "ifnewer"

    def __str__(self):
        return self.name


# TODO: define this. Samples: "20 days" or "1 months"
bareosTime = Annotated[str, Field()]
bareosSpeed = Annotated[str, Field()]
# TODO: define this. Samples: 300, 10G
bareosSize = Annotated[str, Field()]
bareosACL = Annotated[str, Field()]


class jobStatus(str, Enum):
    """
    Allowed job status chars
    """

    A = "A"
    B = "B"
    C = "C"
    D = "D"
    E = "E"
    F = "F"
    I = "I"
    L = "L"
    M = "M"
    R = "R"
    S = "S"
    T = "T"
    W = "W"
    a = "a"
    c = "c"
    d = "d"
    e = "e"
    f = "f"
    i = "i"
    j = "j"
    l = "l"
    m = "m"
    p = "p"
    q = "q"
    s = "s"
    t = "t"

    def __str__(self):
        return self.name


class jobLevelChr(str, Enum):
    """
    Allowed job level chars
    """

    F = "F"
    I = "I"
    D = "D"
    S = "S"
    C = "C"
    V = "V"
    O = "O"
    d = "d"
    A = "A"
    B = "B"
    f = "f"

    def __str__(self):
        return self.name


class jobLevel(str, Enum):
    """
    Allowed job level full description strings
    """

    Base = "Base"
    Full = "Full"
    Incremental = "Incremental"
    Differential = "Differential"
    Since = "Since"
    VerifyCatalog = "Verify Catalog"
    InitCatalog = "Init Catalog"
    VolumetoCatalog = "Volume to Catalog"

    def __str__(self):
        return self.name


class aclCollection(BaseModel):
    """
    Possible ACL options
    """

    jobacl: Optional[bareosACL] = Field(None, title="")
    clientacl: Optional[bareosACL] = Field(None, title="")
    storageacl: Optional[bareosACL] = Field(None, title="")
    scheduleacl: Optional[bareosACL] = Field(None, title="")
    poolacl: Optional[bareosACL] = Field(None, title="")
    commandacl: Optional[bareosACL] = Field(None, title="")
    filesetacl: Optional[bareosACL] = Field(None, title="")
    catalogacl: Optional[bareosACL] = Field(None, title="")
    whereacl: Optional[bareosACL] = Field(None, title="")
    pluginoptionsacl: Optional[bareosACL] = Field(None, title="")


class tlsSettings(BaseModel):
    """
    Options for TLS settings
    """

    tlsauthenticate: Optional[bareosBool] = Field(None, title="")
    tlsenable: Optional[bareosBool] = Field(None, title="")
    tlsrequire: Optional[bareosBool] = Field(None, title="")
    tlscipherlist: Optional[pathlib.Path] = Field(None, title="")
    tlsdhfile: Optional[pathlib.Path] = Field(None, title="")
    tlsverifypeer: Optional[bareosBool] = Field(None, title="")
    tlscacertificatefile: Optional[pathlib.Path] = Field(None, title="")
    tlscacertificatedir: Optional[pathlib.Path] = Field(None, title="")
    tlscertificaterevocationlist: Optional[pathlib.Path] = Field(None, title="")
    tlscertificate: Optional[pathlib.Path] = Field(None, title="")
    tlskey: Optional[pathlib.Path] = Field(None, title="")
    tlsallowedcn: Optional[List[str]] = Field(None, title="")


class volumeQuery(BaseModel):
    """
    Allowed query fields for volume queries
    """

    volume: Optional[str] = Field(
        None, title="volume name to query", example="myvolume"
    )
    jobid: Optional[int] = Field(
        None, title="Search for volumes used by a certain job", example="1", gt=1
    )
    ujobid: Optional[str] = Field(
        None,
        title="Search for volumes used by a certain job given by full job-name",
        example="DefaultJob.2020-08-20_12.56.27_25",
    )
    pool: Optional[str] = Field(
        None, title="Query volumes from the given pool", example="Full"
    )


class volumeLabelDef(BaseModel):
    """
    Options for volume label operation
    """

    volume: str = Field(..., title="Name for the new volume", example="Full-1742")
    pool: str = Field(..., title="New volume will get into this pool", example="Full")
    storage: Optional[str] = Field(None, title="Storage for this volume")
    slot: Optional[int] = Field(None, title="Slot for this volume", ge=0)
    drive: Optional[int] = Field(None, title="Drive number", ge=0)
    # TODO: handle [ barcodes ] [ encrypt ] flags


class volumeRelabelDef(BaseModel):
    """
    Options for volume relabel operation
    """

    volume: str = Field(..., title="New Name for Volume", example="Full-001742")
    storage: str = Field(..., title="Volume's storage", example="File")
    pool: str = Field(
        ...,
        title="Volume's pool, can stay here or be moved to this pool",
        example="Full",
    )
    encrypt: Optional[bareosFlag] = Field(None, title="Encrypt volume", example="yes")


class volumeProperties(BaseModel):
    """
    Volume properties that can be set for _update volume_ operation
    """

    pool: Optional[str] = Field(None, title="New pool for this volume", example="Full")
    slot: Optional[int] = Field(None, title="New slot for this volume", ge=0)
    volstatus: Optional[str] = Field(
        None, title="New status for this volume", example="Archive"
    )
    volretention: Optional[bareosTime] = Field(
        None, title="Volume retention time", example="1 month"
    )
    actiononpurge: Optional[str] = Field(None, title="Action to execute on purge")
    recycle: Optional[bareosBool] = Field(None, title="Set recycle flag")
    inchanger: Optional[bareosBool] = Field(None, title="Set inchanger flag")
    maxvolbytes: Optional[bareosSize] = Field(
        None, title="Set max byte size for this volume", example="20G"
    )
    maxvolfiles: Optional[int] = Field(
        None, title="Set max number of files for this volume", ge=0, example="10000"
    )
    maxvoljobs: Optional[int] = Field(
        None, title="Set max number of jobs for this volume", ge=0, example="20"
    )
    enabled: Optional[bareosBool] = Field(None, title="Enable / disable volume")
    recyclepool: Optional[str] = Field(
        None, title="Define recyclepool for this volume", example="Scratch"
    )


class volumeMove(BaseModel):
    # parameters used for move
    storage: str = Field(..., title="Storage to use for move operation")
    srcslots: str = Field(..., title="Source slot selection")
    dstslots: str = Field(..., title="Destination slot selection")


class volumeExport(BaseModel):
    storage: str = Field(..., title="Storage to use for move operation")
    srcslots: str = Field(..., title="Source slot selection")
    dstslots: Optional[str] = Field(None, title="Destination slot selection")
    volume: Optional[str] = Field(
        None, title="Volume selection", example="A00020L4|A00007L4|A00005L4"
    )
    scan: Optional[bareosFlag] = Field(None, title="scan volume")


class volumeImport(BaseModel):
    storage: str = Field(..., title="Storage to use for import operation")
    srcslots: Optional[str] = Field(..., title="Source slot selection")
    dstslots: Optional[str] = Field(None, title="Destination slot selection")
    volume: Optional[str] = Field(None, title="Volume name")
    scan: Optional[bareosFlag] = Field(None, title="scan volume")


class poolResource(BaseModel):
    name: str = Body(..., title="Pool name")
    description: Optional[str] = Field(None, title="")
    pooltype: Optional[str] = Field(None, title="")
    labelformat: Optional[str] = Field(None, title="")
    labeltype: Optional[str] = Field(None, title="")
    cleaningprefix: Optional[str] = Field(None, title="")
    usecatalog: Optional[bareosBool] = Field(None, title="")
    purgeoldestvolume: Optional[bareosBool] = Field(None, title="")
    actiononpurge: Optional[str] = Field(None, title="")
    recycleoldestvolume: Optional[bareosBool] = Field(None, title="")
    recyclecurrentvolume: Optional[bareosBool] = Field(None, title="")
    maximumvolumes: Optional[int] = Field(None, title="", ge=1)
    maximumvolumejobs: Optional[int] = Field(None, title="", ge=1)
    maximumvolumefiles: Optional[int] = Field(None, title="", ge=1)
    maximumvolumebytes: Optional[int] = Field(None, title="")
    catalogfiles: Optional[bareosBool] = Field(None, title="")
    volumeretention: Optional[bareosTime] = Field(None, title="")
    volumeuseduration: Optional[bareosTime] = Field(None, title="")
    migrationtime: Optional[bareosTime] = Field(None, title="")
    migrationhighbytes: Optional[int] = Field(None, title="")
    migrationlowbytes: Optional[int] = Field(None, title="")
    nextpool: Optional[str] = Field(None, title="")
    storage: Optional[List[str]] = Field(None, title="")
    autoprune: Optional[bareosBool] = Field(None, title="")
    recycle: Optional[bareosBool] = Field(None, title="")
    recyclepool: Optional[str] = Field(None, title="")
    scratchpool: Optional[str] = Field(None, title="")
    catalog: Optional[str] = Field(None, title="")
    fileretention: Optional[bareosTime] = Field(None, title="")
    jobretention: Optional[bareosTime] = Field(None, title="")
    minimumblocksize: Optional[int] = Field(None, title="")
    maximumblocksize: Optional[int] = Field(None, title="")


class scheduleResource(BaseModel):
    name: str = Body(..., title="Schedule name")
    description: Optional[str] = Body(None, title="Schedule Description")
    runCommand: Optional[List[str]] = Body(
        None,
        title="A list of run statements",
        example=["Full 1st Sat at 12:00", "Incremental Sun-Fri at 11:00"],
    )
    enabled: Optional[bareosBool] = Body(
        None, title="Schedule enabled? Yes, if unset", example="no"
    )


class profileResource(aclCollection):
    name: str = Field(..., title="resource name")
    description: Optional[str] = Field(None, title="Description")


class userResource(profileResource):
    profile: Optional[List[str]] = Field(
        None, title="List of profile names for this user"
    )


class jobRange(BaseModel):
    days: Optional[int] = Field(
        None, title="Query jobs run max days ago", gt=1, example=7
    )
    hours: Optional[int] = Field(
        None, title="Query jobs run max hours ago", gt=1, example=12
    )
    since_jobid: Optional[int] = Field(
        None, title="Run all jobs since the given job by id", ge=1
    )
    unitl_jobid: Optional[int] = Field(
        None, title="Run all jobs until the given job by id", ge=1
    )


# class bareosTimeSpec(BaseModel):
#    # TODO: better specification / Regex?
#    timeSpec: str = Field(None, title="Bareos universal time specification")


class jobControl(BaseModel):
    job: str = Field(..., title="Job name to run", example="myjob")
    client: str = Field(None, title="Client to run job on", example="myclient-fd")
    fileset: Optional[str] = Field(
        None, title="Fileset to use", example="server-fileset"
    )
    level: Optional[jobLevel] = Field(None, title="Job level to query", example="Full")
    storage: Optional[str] = Field(None, title="Storage to use")
    when: Optional[str] = Field(
        None, title="When to run job, Bareos universal timespec"
    )
    pool: Optional[str] = Field(None, title="Pool to use", example="LTO-Pool")
    pluginoptions: Optional[str] = Field(
        None, title="Overwrite eventual plugin options"
    )
    accurate: Optional[bareosBool] = Field(None, title="Set / unset accurate option")
    comment: Optional[str] = Field(None, title="Comment")
    spooldata: Optional[bareosBool] = Field(None, title="Spooling")
    priority: Optional[str] = Field(
        None, title="Priority, higher number means lower prio"
    )
    catalog: Optional[str] = Field(None, title="Catalog to use for this job")
    # migrationjob in help run list but not in documentation
    migrationjob: Optional[str] = Field(None)
    backupformat: Optional[str] = Field(
        None,
        title="The backup format used for protocols which support multiple formats.",
    )
    nextpool: Optional[str] = Field(
        None,
        title="A Next Pool override used for Migration/Copy and Virtual Backup Jobs.",
    )
    # since in help run list but not in documentation
    since: Optional[str] = Field(
        None,
        title="Set since time for differential / incremental jobs. Bareos universal timespec",
    )
    verifyjob: Optional[str] = Field(None)
    verifylist: Optional[str] = Field(None)
    migrationjob: Optional[str] = Field(None)


class restoreJobControl(BaseModel):
    client: str = Field(..., title="Restore data from this client")
    where: Optional[pathlib.Path] = Field(
        None, title="Filesystem prefix. Use _/_ for original location", example="/"
    )
    storage: Optional[str] = Field(None, title="")
    bootstrap: Optional[str] = Field(None, title="")
    restorejob: Optional[str] = Field(None, title="")
    comment: Optional[str] = Field(None, title="")
    jobid: Optional[int] = Field(
        None, title="Restore all files backuped by a given jobid", ge=1
    )
    fileset: Optional[str] = Field(None, title="")
    replace: Optional[bareosReplaceOption] = Field(
        None, title="Set file-replace options", example="ifnewer"
    )
    pluginoptions: Optional[str] = Field(None, title="")
    regexwhere: Optional[str] = Field(None, title="")
    restoreclient: Optional[str] = Field(None, title="Restore data to this client")
    backupformat: Optional[str] = Field(None, title="")
    pool: Optional[str] = Field(None, title="")
    file: Optional[str] = Field(None, title="")
    # can't use pathlib.Path here, as it strips trailing /, which is required by Bareos to accept a path
    directory: Optional[str] = Field(None, title="")
    before: Optional[str] = Field(None, title="")
    strip_prefix: Optional[str] = Field(None, title="")
    add_prefix: Optional[str] = Field(None, title="")
    add_suffix: Optional[str] = Field(None, title="")
    select: Optional[str] = Field(None, title="use select=date")
    selectAllDone: Optional[bareosBool] = Field(
        None, title="Run restore job with _select all done_ option"
    )


class jobResource(BaseModel):
    # TODO: complete field description 'title' and find meaningful examples
    messages: str = Field(..., title="Message resource identifier", example="Standard")
    name: str = Field(..., title="Name for this resource", example="DefaultJob")
    pool: str = Field(..., title="Pool for this job", example="Full")
    jobdefs: str = Field(..., title="Jobdefs to use", example="DefaultJob")
    type: str = Field(..., title="Job type", example="Backup")
    accurate: Optional[bareosBool] = Field(
        None,
        title="Accurate setting, will be default 'no' if unset here",
        example="yes",
    )
    addprefix: Optional[str] = Field(None, title="")
    addsuffix: Optional[str] = Field(None, title="")
    allowduplicatejobs: Optional[bareosBool] = Field(None, title="")
    allowhigherduplicates: Optional[bareosBool] = Field(None, title="")
    allowmixedpriority: Optional[bareosBool] = Field(None, title="")
    alwaysincremental: Optional[bareosBool] = Field(None, title="")
    alwaysincrementaljobretention: Optional[bareosTime] = Field(
        None, title="", example="20 days"
    )
    alwaysincrementalkeepnumber: Optional[int] = Field(None, title="", ge=1, example=5)
    cancellowerlevelduplicates: Optional[bareosBool] = Field(None, title="")
    cancelqueuedduplicates: Optional[bareosBool] = Field(None, title="")
    cancelrunningduplicates: Optional[bareosBool] = Field(None, title="")
    catalog: Optional[str] = Field(None, title="")
    client: Optional[str] = Field(None, title="")
    clientrunafterjob: Optional[str] = Field(None, title="")
    clientrunbeforejob: Optional[str] = Field(None, title="")
    description: Optional[str] = Field(None, title="")
    differentialbackuppool: Optional[str] = Field(None, title="")
    differentialmaxruntime: Optional[bareosTime] = Field(None, title="")
    dirpluginoptions: List[str] = Field(None, title="")
    enabled: Optional[bareosBool] = Field(None, title="")
    fdpluginoptions: List[str] = Field(None, title="")
    filehistorysize: Optional[int] = Field(None, title="", gt=1)
    fileset: Optional[str] = Field(None, title="")
    fullbackuppool: Optional[str] = Field(None, title="")
    fullmaxruntime: Optional[bareosTime] = Field(None, title="")
    incrementalbackuppool: Optional[str] = Field(None, title="")
    incrementalmaxruntime: Optional[bareosTime] = Field(None, title="")
    jobtoverify: Optional[str] = Field(None, title="")
    level: Optional[jobLevel] = Field(None, title="Job Level", example="Full")
    maxconcurrentcopies: Optional[int] = Field(None, title="", gt=1)
    maxdiffinterval: Optional[bareosTime] = Field(None, title="")
    maxfullconsolidations: Optional[int] = Field(None, title="", gt=1)
    maxfullinterval: Optional[bareosTime] = Field(None, title="")
    maximumbandwidth: Optional[bareosSpeed] = Field(None, title="")
    maximumconcurrentjobs: Optional[int] = Field(None, title="")
    maxrunschedtime: Optional[bareosTime] = Field(None, title="")
    maxruntime: Optional[bareosTime] = Field(None, title="")
    maxstartdelay: Optional[bareosTime] = Field(None, title="")
    maxvirtualfullinterval: Optional[bareosTime] = Field(None, title="")
    maxwaittime: Optional[bareosTime] = Field(None, title="")
    nextpool: Optional[str] = Field(None, title="")
    prefermountedvolumes: Optional[bareosBool] = Field(None, title="")
    prefixlinks: Optional[bareosBool] = Field(None, title="")
    priority: Optional[int] = Field(None, title="", gt=1)
    protocol: Optional[str] = Field(None, title="")
    prunefiles: Optional[bareosBool] = Field(None, title="")
    prunejobs: Optional[bareosBool] = Field(None, title="")
    prunevolumes: Optional[bareosBool] = Field(None, title="")
    purgemigrationjob: Optional[bareosBool] = Field(None, title="")
    regexwhere: Optional[str] = Field(None, title="")
    replace: Optional[str] = Field(None, title="")
    rerunfailedlevels: Optional[bareosBool] = Field(None, title="")
    rescheduleinterval: Optional[bareosTime] = Field(None, title="")
    rescheduleonerror: Optional[bareosBool] = Field(None, title="")
    rescheduletimes: Optional[int] = Field(None, title="", gt=1)
    runafterfailedjob: Optional[str] = Field(None, title="")
    runafterjob: Optional[str] = Field(None, title="")
    runbeforejob: Optional[str] = Field(None, title="")
    runonincomingconnectinterval: Optional[bareosTime] = Field(None, title="")
    run: List[str] = Field(None, title="")
    savefilehistory: Optional[bareosBool] = Field(None, title="")
    schedule: Optional[str] = Field(None, title="")
    sdpluginoptions: List[str] = Field(None, title="")
    selectionpattern: Optional[str] = Field(None, title="")
    selectiontype: Optional[str] = Field(None, title="", example="OldestVolume")
    spoolattributes: Optional[bareosBool] = Field(None, title="")
    spooldata: Optional[bareosBool] = Field(None, title="")
    spoolsize: Optional[int] = Field(None, title="", gt=1)
    storage: List[str] = Field(None, title="")
    stripprefix: Optional[str] = Field(None, title="")
    virtualfullbackuppool: Optional[str] = Field(None, title="")
    where: Optional[pathlib.Path] = Field(None, title="")
    writebootstrap: Optional[pathlib.Path] = Field(None, title="")
    writeverifylist: Optional[pathlib.Path] = Field(None, title="")


class jobDefs(jobResource):
    jobdefs: Optional[str] = Field(None, title="Jobdefs to use", example="DefaultJob")


class clientResource(tlsSettings):
    name: str
    address: str
    password: str
    description: Optional[str] = Field(None, title="A client description")
    passive: Optional[bareosBool] = Field(
        None,
        title="Passive clients will wait for Director and SD to open any connection",
    )
    port: Optional[int] = Field(None, gt=1, le=65535)
    protocol: Optional[str] = Field(None)
    authtype: Optional[str] = Field(None)
    lanaddress: Optional[str] = Field(None)
    username: Optional[str] = Field(None)
    catalog: Optional[str] = Field(None)
    connectionfromdirectortoclient: Optional[bareosBool] = Field(None)
    connectionfromclienttodirector: Optional[bareosBool] = Field(None)
    enabled: Optional[bareosBool] = Field(None)
    hardquota: Optional[int] = Field(None)
    softquota: Optional[int] = Field(None)
    softquotagraceperiod: Optional[str] = Field(None)
    strictquotas: Optional[bareosBool] = Field(None)
    quotaincludefailedjobs: Optional[bareosBool] = Field(None)
    fileretention: Optional[str] = Field(None)
    jobretention: Optional[str] = Field(None)
    heartbeatinterval: Optional[str] = Field(None)
    autoprune: Optional[bareosBool] = Field(None)
    maximumconcurrentjobs: Optional[PositiveInt] = Field(None)
    maximumbandwidthperjob: Optional[str] = Field(None)
    ndmploglevel: Optional[PositiveInt] = Field(None)
    ndmpblocksize: Optional[PositiveInt] = Field(None)
    ndmpuselmdb: Optional[bareosBool] = Field(None)


class deviceResource(tlsSettings):
    device: str = Field(..., title="")
    mediatype: str = Field(..., title="")
    description: Optional[str] = Field(None, title="")
    protocol: Optional[str] = Field(None, title="")
    authtype: Optional[str] = Field(None, title="")
    lanaddress: Optional[str] = Field(None, title="")
    port: Optional[int] = Field(None, title="", ge=1)
    username: Optional[str] = Field(None, title="")
    autochanger: Optional[bareosBool] = Field(None, title="")
    enabled: Optional[bareosBool] = Field(None, title="")
    allowcompression: Optional[bareosBool] = Field(None, title="")
    heartbeatinterval: Optional[bareosTime] = Field(None, title="")
    cachestatusinterval: Optional[bareosTime] = Field(None, title="")
    maximumconcurrentjobs: Optional[int] = Field(None, title="", ge=1)
    maximumconcurrentreadjobs: Optional[int] = Field(None, title="", ge=1)
    pairedstorage: Optional[str] = Field(None, title="")
    maximumbandwidthperjob: Optional[bareosSpeed] = Field(None, title="")
    collectstatistics: Optional[bareosBool] = Field(None, title="")
    ndmpchangerdevice: Optional[str] = Field(None, title="")


class storageResource(deviceResource):
    name: str = Field(..., title="")
    address: str = Field(..., title="")
    password: str = Field(..., title="")


class consoleResource(userResource, tlsSettings):
    password: str = Field(..., title="Console password")


class jobQuery(BaseModel):
    job: Optional[str] = Field(None, title="Job name to query", example="myjob")
    client: Optional[str] = Field(None, title="Client to query", example="myclient-fd")
    jobstatus: Optional[jobStatus] = Field(
        None, title="Job status to query", example="T"
    )
    joblevel: Optional[jobLevelChr] = Field(
        None, title="Job level to query", example="F"
    )
    volume: Optional[str] = Field(
        None, title="Query jobs on the given volume", example="Full-0017"
    )
    days: Optional[int] = Field(
        None, title="Query jobs run max days ago", gt=1, example=7
    )
    hours: Optional[int] = Field(
        None, title="Query jobs run max hours ago", gt=1, example=12
    )
