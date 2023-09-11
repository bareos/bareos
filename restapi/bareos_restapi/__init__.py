#!/usr/bin/env python
# -*- coding: utf-8 -*-

#
# BAREOS - Backup Archiving REcovery Open Sourced
#
# Copyright (C) 2020-2023 Bareos GmbH & Co. KG
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
# Author: Maik Aussendorf
#

import configparser
from datetime import datetime, timedelta
from fastapi import Depends, FastAPI, HTTPException, status, Response, Path, Body, Query
from fastapi.security import OAuth2PasswordBearer, OAuth2PasswordRequestForm
from jose import JWTError, jwt
import os
from packaging import version
from passlib.context import CryptContext
from pydantic import BaseModel
from typing import Optional
import yaml

import bareos.bsock
import bareos.util
from bareos_restapi.models import *

# Read config from api.ini
config = configparser.ConfigParser()
config.read("api.ini")

CONFIG_DIRECTOR_ADDRESS = config.get("Director", "Address")
CONFIG_DIRECTOR_NAME = config.get("Director", "Name")
CONFIG_DIRECTOR_PORT = config.getint("Director", "Port")

SECRET_KEY = config.get("JWT", "secret_key")
ALGORITHM = config.get("JWT", "algorithm")
ACCESS_TOKEN_EXPIRE_MINUTES = config.getint("JWT", "access_token_expire_minutes")

userDirectors = {}
users_db = {}

# Load metatags.yaml from the same directory.
with open(
    os.path.dirname(os.path.realpath(__file__)) + "/metatags.yaml", "r"
) as stream:
    tags_metadata = yaml.safe_load(stream)

pwd_context = CryptContext(schemes=["bcrypt"], deprecated="auto")

oauth2_scheme = OAuth2PasswordBearer(tokenUrl="token")

app = FastAPI(
    title="Bareos REST API",
    description="Bareos REST API built on python-bareos. Experimental and subject to enhancements and changes. **Note** swagger does not support GET methods with bodies, however, the CURL statements displayed by swagger do work.",
    version="0.0.1",
    openapi_tags=tags_metadata,
)


class UserObject(object):
    def __init__(self, username, password):
        # self.id = id
        self.username = username
        self.password = password
        self.directorName = CONFIG_DIRECTOR_NAME
        self.director = bareos.bsock.BSock
        self.jsonDirector = bareos.bsock.DirectorConsoleJson
        self.directorVersion = ""  # Format: xx.yy.zz, example: 19.02.06

    def __str__(self):
        return "User(username='%s')" % (self.username)

    def __iter__(self):
        yield "username", self.username
        yield "directorName", self.directorName
        yield "directorVersion", self.directorVersion

    def getDirectorVersion(self):
        return self.directorVersion


def verify_password(plain_password, hashed_password):
    return pwd_context.verify(plain_password, hashed_password)


def get_password_hash(password):
    return pwd_context.hash(password)


def get_user(username: str):
    if username in users_db:
        # print(users_db[username])
        # return {'username':username, 'directorName': users_db[username].directorName}
        return users_db[username]


def authenticate_user(username: str, password: str):
    jsonDirector = None
    try:
        jsonDirector = bareos.bsock.DirectorConsoleJson(
            address=CONFIG_DIRECTOR_ADDRESS,
            port=CONFIG_DIRECTOR_PORT,
            dirname=CONFIG_DIRECTOR_NAME,
            name=username,
            password=bareos.bsock.Password(password),
        )
    except Exception as e:
        print(
            "Could not authorize %s at director %s. %s"
            % (username, CONFIG_DIRECTOR_NAME, e)
        )
        return False
    user = UserObject(username, password)
    user.jsonDirector = jsonDirector
    user.username = username
    users_db[username] = user
    return user


def create_access_token(data: dict, expires_delta: Optional[timedelta] = None):
    to_encode = data.copy()
    if expires_delta:
        expire = datetime.utcnow() + expires_delta
    else:
        expire = datetime.utcnow() + timedelta(minutes=15)
    to_encode.update({"exp": expire})
    encoded_jwt = jwt.encode(to_encode, SECRET_KEY, algorithm=ALGORITHM)
    return encoded_jwt


async def get_current_user(token: str = Depends(oauth2_scheme)):
    credentials_exception = HTTPException(
        status_code=status.HTTP_401_UNAUTHORIZED,
        detail="Could not validate credentials",
        headers={"WWW-Authenticate": "Bearer"},
    )
    try:
        payload = jwt.decode(token, SECRET_KEY, algorithms=[ALGORITHM])
        username: str = payload.get("sub")
        if username is None:
            raise credentials_exception
        token_data = TokenData(username=username)
    except JWTError:
        raise credentials_exception
    user = get_user(username=token_data.username)
    if user is None:
        raise credentials_exception
    return user


@app.post("/token", response_model=Token)
async def login_for_access_token(form_data: OAuth2PasswordRequestForm = Depends()):
    user = authenticate_user(form_data.username, form_data.password)
    if not user:
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="Incorrect username or password",
            headers={"WWW-Authenticate": "Bearer"},
        )
    access_token_expires = timedelta(minutes=ACCESS_TOKEN_EXPIRE_MINUTES)
    access_token = create_access_token(
        data={"sub": user.username}, expires_delta=access_token_expires
    )
    return {"access_token": access_token, "token_type": "bearer"}


@app.get("/users/me/", response_model=User)
async def read_users_me(current_user: User = Depends(get_current_user)):
    return current_user


## Generic Methods


def versionCheck(
    *,
    response: Response,
    current_user: User = Depends(get_current_user),
    minVersion: Optional[str] = "16.1.1",
):
    myVersion = ""
    if current_user.directorVersion > "":
        myVersion = current_user.directorVersion
    else:
        result = read_director_version(response=response, current_user=current_user)
        if "version" in result:
            myVersion = bareos.util.Version(result["version"]).as_python_version()
            current_user.directorVersion = myVersion
        else:
            raise HTTPException(
                status_code=500,
                detail="Could not read version from director. Need at least version %s"
                % (minVersion),
            )
    try:
        if not (version.parse(myVersion) >= version.parse(minVersion)):
            raise HTTPException(
                status_code=501,
                detail="Not implemented in Bareos %s. Need at least version %s"
                % (myVersion, minVersion),
            )
        else:
            return True
    except version.InvalidVersion as exception:
        raise HTTPException(status_code=500, detail=str(exception))


def configure_add_standard_component(
    *,
    componentDef: BaseModel,
    response: Response,
    current_user: User = Depends(get_current_user),
    componentType=str,
):
    """
    Create a new Bareos standard component resource.
    Console command used: _configure add component_

    """

    addCommand = "configure add %s" % componentType
    componentDict = componentDef.dict()

    for a in componentDict:
        if componentDict[a] is not None:
            addCommand += " %s=%s" % (
                a,
                str(componentDict[a]).strip("[]").replace("'", "").replace(" ", ""),
            )
    # print(addCommand)
    # print(current_user)
    try:
        result = current_user.jsonDirector.call(addCommand)
    except Exception as e:
        response.status_code = 500
        return {
            "message": "Could not add %s with command '%s'. Message: '%s'"
            % (componentType, addCommand, e)
        }

    if "configure" in result and "add" in result["configure"]:
        return result
    else:
        response.status_code = 500
        return {
            "message": "Could not add %s with command '%s' on director %s. Message: '%s'"
            % (componentType, addCommand, current_user.directorName, e)
        }


def switch_resource(
    *,
    response: Response,
    current_user: User = Depends(get_current_user),
    resourceName: str,
    componentType=str,
    enable=bool,
):
    """
    Enables or disables a client, job or schedule
    Returns tuple: success(bool), json-return-string(dict)
    """
    responseDict = {}
    if enable:
        command = "enable "
    else:
        command = "disable "
    command += "%s=%s" % (componentType, resourceName)
    # print(command)
    try:
        responseDict = current_user.jsonDirector.call(command)
    except Exception as e:
        response.status_code = 500
        return (
            False,
            {
                "message": "Could not en/disable %s %s on director %s. Message: '%s'"
                % (componentType, resourceName, CONFIG_DIRECTOR_NAME, e)
            },
        )
    response.status_code = 200
    return (True, responseDict)


def parseCommandOptions(queryDict):
    optionString = ""
    for q in queryDict:
        if queryDict[q] is not None:
            if type(queryDict[q]) != bareosFlag:
                optionString += " %s=%s" % (q, str(queryDict[q]))
            else:
                if queryDict[q]:
                    optionString += " %s" % q
    return optionString


def show_configuration_items(
    *,
    response: Response,
    current_user: User = Depends(get_current_user),
    itemType: str,
    byName: Optional[str] = None,
    verbose: Optional[bareosBool] = "yes",
):
    """
    Uses _show_ command to provide configuration setting
    """
    versionCheck(
        response=response,
        current_user=current_user,
        minVersion="20.0.1",
    )
    # Sometimes config type identificator differs from key returned by director, we need to map
    itemKey = itemType
    # itemTypeKeyMap = {"clients":"client", "jobs":"job", "pools":"pool", "schedules": "schedule", "storages":"storage", "users":"user", "profiles":"profile", "consoles":"console"}
    # if itemType in itemTypeKeyMap:
    #    itemKey = itemTypeKeyMap[itemType]

    foundItems = 0
    showCommand = "show %s" % itemType
    if byName:
        showCommand += "=%s" % byName
    if verbose:
        # print ("verbose on")
        showCommand += " verbose"
    try:
        responseDict = current_user.jsonDirector.call(showCommand)
    except Exception as e:
        raise HTTPException(
            status_code=500,
            detail="Could not read %s from director %s. Message: '%s'"
            % (itemType, CONFIG_DIRECTOR_NAME, e),
        )
    # print(responseDict)
    if itemKey in responseDict:
        foundItems = len(responseDict[itemKey])
    if foundItems > 0 and byName:
        return responseDict[itemKey]
    elif foundItems > 0:
        return {"totalItems": foundItems, itemType: responseDict[itemKey]}
    else:
        raise HTTPException(status_code=404, detail="No %s found." % itemKey)


def list_catalog_items(
    *,
    response: Response,
    current_user: User = Depends(get_current_user),
    itemType: str,
    limit: Optional[int] = None,
    offset: Optional[int] = None,
    jobQuery: Optional[jobQuery] = None,
    verbose: Optional[bareosBool] = "yes",
    hasCountOption: Optional[bareosBool] = "no",
):
    itemKey = itemType
    itemTypeKeyMap = {"files": "filenames"}
    if itemType in itemTypeKeyMap:
        itemKey = itemTypeKeyMap[itemType]
    if verbose:
        listCommand = "llist "
    else:
        listCommand = "list "
    listCommand += itemType
    queryDict = {}
    countDict = {}
    results = {}
    countCommand = listCommand
    if jobQuery is not None:
        queryDict = jobQuery.dict()
    for q in queryDict:
        if queryDict[q] is not None:
            listCommand += " %s=%s" % (q, str(queryDict[q]))
            countCommand += " %s=%s" % (q, str(queryDict[q]))
    if limit is not None:
        listCommand += " limit=%d" % limit
    if offset is not None:
        listCommand += " offset=%d" % offset
    countCommand += " count"
    try:
        responseDict = current_user.jsonDirector.call(listCommand)
        if hasCountOption:
            countDict = current_user.jsonDirector.call(countCommand)
    except Exception as e:
        raise HTTPException(
            status_code=500,
            detail={
                "message": "Could not read %s list from director %s. Message: '%s'"
                % (itemType, CONFIG_DIRECTOR_NAME, e)
            },
        )
    foundItems = len(responseDict)
    if (
        hasCountOption == "yes"
        and itemKey in countDict
        and "count" in countDict[itemKey][0]
    ):
        results["totalItems"] = countDict[itemKey][0]["count"]
    else:
        results["totalItems"] = foundItems
    if foundItems > 0:
        # check for limit / offset
        if limit is not None:
            results["limit"] = limit
        if offset is not None:
            results["offset"] = offset
        return {**results, itemKey: responseDict[itemKey]}
    else:
        raise HTTPException(
            status_code=404, detail={"message": "No %s found." % itemType}
        )


### Clients ###
@app.get("/control/clients", status_code=200, tags=["clients", "control"])
def read_catalog_info_for_all_clients(
    response: Response,
    current_user: User = Depends(get_current_user),
    name: Optional[str] = None,
):
    """
    Read status information from catalog about all clients or just one client by name.
    Built on console command _llist client_
    """
    if name:
        listCommand = "llist client=%s" % name
    else:
        listCommand = "llist clients"
    try:
        responseDict = current_user.jsonDirector.call(listCommand)
    except Exception as e:
        response.status_code = 500
        return {
            "message": "Could not read client list from director %s. Message: '%s'"
            % (CONFIG_DIRECTOR_NAME, e)
        }
    if "clients" in responseDict:
        totalItems = len(responseDict["clients"])
        return {"totalItems": totalItems, "clients": responseDict["clients"]}
    else:
        response.status_code = 404
        return {"message": "No clients found."}


@app.get("/control/clients/{client_id}", tags=["clients", "control"])
def read_catalog_info_for_particular_client(
    *,
    client_id: int = Path(..., title="The ID of client to get", ge=1),
    response: Response,
    current_user: User = Depends(get_current_user),
):
    """
    Read catalog information for one client by id.
    Built on console command _llist client_

    **Warning** Director does not support direct query by _id_ we query all clients and filter the result.
    Maybe more time consuming than expected in large settings.
    """
    allClients = read_catalog_info_for_all_clients(response, current_user)
    result = None
    for c in allClients["clients"]:
        if c["clientid"] == str(client_id):
            result = c
            break
    if result:
        return result
    else:
        response.status_code = 404
        return {
            "message": "Client with Client ID {client_id} not found".format(
                client_id=client_id
            )
        }
    return {"item_id": item_id}


@app.put(
    "/control/clients/enable/{client_name}",
    status_code=204,
    tags=["clients", "control"],
)
def enable_client(
    *,
    client_name: str = Path(..., title="The client (name) to enable"),
    response: Response,
    current_user: User = Depends(get_current_user),
):
    (result, jsonMessage) = switch_resource(
        response=response,
        current_user=current_user,
        resourceName=client_name,
        componentType="client",
        enable=True,
    )
    return jsonMessage


@app.put(
    "/control/clients/disable/{client_name}",
    status_code=204,
    tags=["clients", "control"],
)
def disable_client(
    *,
    client_name: str = Path(..., title="The client (name) to disable"),
    response: Response,
    current_user: User = Depends(get_current_user),
):
    (result, jsonMessage) = switch_resource(
        response=response,
        current_user=current_user,
        resourceName=client_name,
        componentType="client",
        enable=False,
    )
    return jsonMessage


@app.get("/configuration/clients", tags=["clients", "configuration"])
def read_all_clients(
    *,
    response: Response,
    current_user: User = Depends(get_current_user),
    verbose: Optional[bareosBool] = Query("yes", title="Verbose output"),
):
    """
    Read all jobdef resources. Built on console command _show clients_.

    Needs at least Bareos Version >= 20.0.0
    """
    return show_configuration_items(
        response=response,
        current_user=current_user,
        itemType="clients",
        verbose=verbose,
    )


@app.get("/configuration/clients/{clients_name}", tags=["clients", "configuration"])
def read_client_by_name(
    *,
    response: Response,
    current_user: User = Depends(get_current_user),
    clients_name: str = Path(..., title="Client name to look for"),
    verbose: Optional[bareosBool] = Query("yes", title="Verbose output"),
):
    """
    Read all jobdef resources. Built on console command _show clients_.

    Needs at least Bareos Version >= 20.0.0
    """
    return show_configuration_items(
        response=response,
        current_user=current_user,
        itemType="clients",
        byName=clients_name,
        verbose=verbose,
    )


@app.post("/configuration/clients", tags=["clients", "configuration"])
def post_client(
    *,
    clientDef: clientResource = Body(..., title="The client to create"),
    response: Response,
    current_user: User = Depends(get_current_user),
):
    return configure_add_standard_component(
        componentDef=clientDef,
        response=response,
        current_user=current_user,
        componentType="client",
    )


### filesets


@app.get("/configuration/filesets", tags=["filesets", "configuration"])
def read_all_filesets(
    *,
    response: Response,
    current_user: User = Depends(get_current_user),
    verbose: Optional[bareosBool] = Query("yes", title="Verbose output"),
):
    """
    Read all jobdef resources. Built on console command _show filesets_.

    Needs at least Bareos Version >= 20.0.0
    """
    return show_configuration_items(
        response=response,
        current_user=current_user,
        itemType="filesets",
        verbose=verbose,
    )


@app.get("/configuration/filesets/{filesets_name}", tags=["filesets", "configuration"])
def read_fileset_by_name(
    *,
    response: Response,
    current_user: User = Depends(get_current_user),
    filesets_name: str = Path(..., title="fileset name to look for"),
    verbose: Optional[bareosBool] = Query("yes", title="Verbose output"),
):
    """
    Read all jobdef resources. Built on console command _show filesets_.

    Needs at least Bareos Version >= 20.0.0
    """
    return show_configuration_items(
        response=response,
        current_user=current_user,
        itemType="filesets",
        byName=filesets_name,
        verbose=verbose,
    )


#### Job Control


@app.post("/control/jobs/run", tags=["jobcontrol", "control", "jobs"])
def runJob(
    *,
    jobControl: jobControl = Body(..., title="Job control information", embed=True),
    response: Response,
    current_user: User = Depends(get_current_user),
):
    """
    Run a job, defined by jobControl record.

    **Note**: Swagger throws a weird error when running this command by the UI,
    while the given curl statement works fine.
    """
    result = None
    jobCommand = "run"
    args = jobControl.dict()
    for a in args:
        if args[a] is not None:
            jobCommand += " %s=%s" % (a, str(args[a]))
    # print(jobCommand)
    try:
        result = current_user.jsonDirector.call(jobCommand)
    except Exception as e:
        response.status_code = 500
        return {
            "message": "Could not start job '%s' on director %s. Message: '%s'"
            % (jobCommand, current_user.directorName, e)
        }
    if "run" in result and "jobid" in result["run"]:
        return {"jobid": int(result["run"]["jobid"])}
    else:
        response.status_code = 500
        return {"message": "Job '%s' triggered but no jobId returned" % jobCommand}


@app.post("/control/jobs/rerun/{job_id}", tags=["jobcontrol", "control", "jobs"])
def rerun_Job_by_jobid(
    *,
    job_id: int = Path(..., title="The ID of job to rerun", ge=1),
    response: Response,
    current_user: User = Depends(get_current_user),
):
    """
    Rerun a specific job given bei jobid
    """
    result = None
    rerunCommand = "rerun jobid=%d" % job_id
    try:
        result = current_user.jsonDirector.call(rerunCommand)
    except Exception as e:
        response.status_code = 500
        return {
            "message": "Could not rerun jobid %d on director %s. Message: '%s'"
            % (job_id, current_user.directorName, e)
        }
    if "run" in result and "jobid" in result["run"]:
        return {"jobid": int(result["run"]["jobid"])}
    else:
        response.status_code = 500
        return {"message": "Job '%s' triggered but no jobId returned" % jobCommand}


@app.post("/control/jobs/rerun", tags=["jobcontrol", "control", "jobs"])
def rerun_Job(
    *,
    job_range: jobRange = Body(..., title="Job range to rerun"),
    response: Response,
    current_user: User = Depends(get_current_user),
):
    """
    Rerun jobs given by the following parameters (at least one)

    - **since_jobid**=_jobid_ rerun failed jobs since this jobid
    - **until_jobid**=_jobid_ - in conjunction with _since_jobid_
    - **days**=_nr_days_ - since a number of days
    - **hours**=_nr_hours_ - since a number of hours

    Built on console command _rerun_
    """
    result = None
    rerunCommand = "rerun"
    args = job_range.dict()
    for a in args:
        if args[a] is not None:
            rerunCommand += " %s=%s" % (a, args[a])
    rerunCommand += " yes"
    try:
        result = current_user.jsonDirector.call(rerunCommand)
    except Exception as e:
        response.status_code = 500
        return {
            "message": "Could not %s on director %s. Message: '%s'"
            % (rerunCommand, current_user.directorName, e)
        }
    # print(result)
    if "run" in result and "jobid" in result["run"]:
        return {"jobid": int(result["run"]["jobid"])}
    else:
        response.status_code = 500
        return {"message": "Job '%s' triggered but no jobId returned" % rerunCommand}


@app.post("/control/jobs/restore", tags=["jobcontrol", "control", "jobs"])
def runRestoreJob(
    *,
    jobControl: restoreJobControl = Body(
        ..., title="Restore Job control information", embed=True
    ),
    response: Response,
    current_user: User = Depends(get_current_user),
):
    """
    Run a restore-job, defined by jobControl record.

    """
    result = None
    jobCommand = "restore"
    args = jobControl.dict()
    for a in args:
        # print(" %s=%s" % (a, str(args[a])))
        if args[a] is not None:
            if a != "selectAllDone":
                jobCommand += " %s=%s" % (a, str(args[a]))
            elif args[a] == "yes":
                jobCommand += " select all done"
    # print(jobCommand)
    try:
        result = current_user.jsonDirector.call(jobCommand)
    except Exception as e:
        response.status_code = 500
        return {
            "message": "Could not start job '%s' on director %s. Message: '%s'"
            % (jobCommand, current_user.directorName, e)
        }
    if "run" in result and "jobid" in result["run"]:
        return {"jobid": int(result["run"]["jobid"])}
    else:
        response.status_code = 500
        return {"message": "Job '%s' triggered but no jobId returned" % jobCommand}


@app.put("/control/jobs/cancel/{job_id}", tags=["jobcontrol", "control", "jobs"])
def cancelJob(
    *,
    job_id: int = Path(..., title="The ID of job to cancel", ge=1),
    response: Response,
    current_user: User = Depends(get_current_user),
):
    """
    Cancel a specific job given bei jobid
    """
    # cancel a specific job given bei jobid
    cancelCommand = "cancel jobid=%d" % job_id
    result = None
    try:
        result = current_user.jsonDirector.call(cancelCommand)
    except Exception as e:
        response.status_code = 500
        return {
            "message": "Could not cancel jobid %d on director %s. Message: '%s'"
            % (job_id, current_user.directorName, e)
        }
    return result


@app.put(
    "/control/jobs/enable/{job_name}",
    status_code=204,
    tags=["jobcontrol", "jobs", "control"],
)
def enable_job(
    *,
    job_name: str = Path(..., title="The job (name) to enable"),
    response: Response,
    current_user: User = Depends(get_current_user),
):
    (result, jsonMessage) = switch_resource(
        response=response,
        current_user=current_user,
        resourceName=job_name,
        componentType="job",
        enable=True,
    )
    return jsonMessage


@app.put(
    "/control/jobs/disable/{job_name}",
    status_code=204,
    tags=["jobcontrol", "jobs", "control"],
)
def disable_job(
    *,
    job_name: str = Path(..., title="The job (name) to disable"),
    response: Response,
    current_user: User = Depends(get_current_user),
):
    (result, jsonMessage) = switch_resource(
        response=response,
        current_user=current_user,
        resourceName=job_name,
        componentType="job",
        enable=False,
    )
    return jsonMessage


#### Job Status


@app.get("/control/jobs/totals", tags=["jobcontrol", "control", "jobs"])
def read_all_jobs_totals(
    *, response: Response, current_user: User = Depends(get_current_user)
):
    listCommand = "llist jobtotals"
    results = {}
    try:
        responseDict = current_user.jsonDirector.call(listCommand)
    except Exception as e:
        response.status_code = 500
        return {
            "message": "Could not read job totals from director %s. Message: '%s'"
            % (CONFIG_DIRECTOR_NAME, e)
        }
    if "jobtotals" in responseDict:
        return responseDict
    else:
        response.status_code = 404
        return {"message": "No jobtotals found."}


@app.get("/control/jobs/{job_id}", tags=["jobcontrol", "control", "jobs"])
def read_job_status(
    *,
    job_id: int = Path(..., title="The ID of job to get", ge=1),
    response: Response,
    current_user: User = Depends(get_current_user),
):
    """
    Read information about a specific job defined by jobid
    Returns output of command _llist jobid=id_
    """
    result = None
    listCommand = "llist jobid=%d" % job_id
    try:
        result = current_user.jsonDirector.call(listCommand)
    except Exception as e:
        response.status_code = 500
        return {
            "message": "Could not query jobs on director %s. Message: '%s'"
            % (current_user.directorName, e)
        }
    if result and "jobs" in result:
        return result["jobs"][0]
    else:
        response.status_code = 404
        return {"message": "Job with Job ID {jobid} not found".format(jobid=job_id)}


@app.get("/control/jobs", tags=["jobcontrol", "control", "jobs"])
def read_all_jobs_status(
    *,
    response: Response,
    current_user: User = Depends(get_current_user),
    limit: Optional[int] = Query(None, title="Result items limit", gt=1),
    offset: Optional[int] = Query(None, title="Result items offset", gt=0),
    jobQuery: Optional[jobQuery] = Body(None, title="Query parameter"),
):
    return list_catalog_items(
        itemType="jobs",
        current_user=current_user,
        response=response,
        limit=limit,
        offset=offset,
        jobQuery=jobQuery,
        hasCountOption="yes",
    )


@app.delete("/control/jobs/{job_id}", tags=["jobcontrol", "control", "jobs"])
def delete_job(
    *,
    job_id: int = Path(..., title="The ID of job to delete", ge=1),
    response: Response,
    current_user: User = Depends(get_current_user),
):
    """
    Delete job record from catalog
    """
    # Director gives no success nor failed information
    # We implemente validation here (check if job exists before and after deletion)
    jobStatusResponse = read_job_status(
        job_id=job_id, response=response, current_user=current_user
    )
    if not "jobid" in jobStatusResponse:
        response.status_code = 404
        return {
            "message": "No job with id %d found on director %s."
            % (job_id, current_user.directorName)
        }
    # delete a specific job record given bei jobid
    deleteCommand = "delete jobid=%d" % job_id
    try:
        result = current_user.jsonDirector.call(deleteCommand)
    except Exception as e:
        response.status_code = 500
        return {
            "message": "Could not delete jobid %d on director %s. Message: '%s'"
            % (job_id, current_identity.directorName, e)
        }
    jobStatusResponse = read_job_status(
        job_id=job_id, response=response, current_user=current_user
    )
    if "jobid" in jobStatusResponse:
        response.status_code = 500
        return {
            "message": "Job with id %d still exists on director %s. Delete failed"
            % (job_id, current_user.directorName)
        }
    response.status_code = 200
    return {"message": "Job %d succesfully deleted." % job_id}


@app.get("/control/jobs/logs/{job_id}", tags=["jobcontrol", "control", "jobs"])
def read_one_job_log(
    *,
    job_id: int = Path(..., title="The ID of job to get the logs", ge=1),
    response: Response,
    current_user: User = Depends(get_current_user),
):
    """
    Read logs from a specific job defined by jobid
    Returns output of command _list joblog jobid=id_
    """
    result = None
    listCommand = "list joblog jobid=%d" % job_id
    try:
        result = current_user.jsonDirector.call(listCommand)
    except Exception as e:
        response.status_code = 500
        return {
            "message": "Could not read joblogs on director %s. Message: '%s'"
            % (current_user.directorName, e)
        }
    if result and "joblog" in result:
        totalItems = len(result["joblog"])
        return {"totalItems": totalItems, "joblog": result["joblog"]}
    else:
        response.status_code = 404
        return {"message": "Joblogs Job with ID {jobid} not found".format(jobid=job_id)}


@app.get("/control/jobs/files/{job_id}", tags=["jobcontrol", "control", "jobs"])
def read_files_of_job(
    *,
    job_id: int = Path(..., title="The ID of job to get the files", ge=1),
    response: Response,
    current_user: User = Depends(get_current_user),
):
    """
    Read files from a specific job defined by jobid
    Returns output of command _list joblog jobid=id_
    """
    result = None
    listCommand = "list files jobid=%d" % job_id
    try:
        result = current_user.jsonDirector.call(listCommand)
    except Exception as e:
        response.status_code = 500
        return {
            "message": "Could not read jobfiles on director %s. Message: '%s'"
            % (current_user.directorName, e)
        }
    if result and "filenames" in result:
        totalItems = len(result["filenames"])
        return {"totalItems": totalItems, "filenames": result["filenames"]}
    else:
        response.status_code = 404
        return {
            "message": "Files for job with ID {jobid} not found".format(jobid=job_id)
        }


#### JobDefs


@app.post("/confguration/jobdefs", tags=["jobdefs", "configuration"])
def post_jobdef(
    *,
    jobDef: jobDefs = Body(..., title="Jobdef resource"),
    response: Response,
    current_user: User = Depends(get_current_user),
):
    """
    Create a new jobdefs resource.
    Console command used: _configure add jobdefs_
    """
    return configure_add_standard_component(
        componentDef=jobDef,
        response=response,
        current_user=current_user,
        componentType="jobdefs",
    )


@app.get("/configuration/jobdefs", tags=["jobdefs", "configuration"])
def read_all_jobdefs(
    *,
    response: Response,
    current_user: User = Depends(get_current_user),
    verbose: Optional[bareosBool] = Query("yes", title="Verbose output"),
):
    """
    Read all jobdef resources. Built on console command _show jobdefs_.

    Needs at least Bareos Version >= 20.0.0
    """
    return show_configuration_items(
        response=response,
        current_user=current_user,
        itemType="jobdefs",
        verbose=verbose,
    )


@app.get("/configuration/jobdefs/{jobdefs_name}", tags=["jobdefs", "configuration"])
def read_jobdef_by_name(
    *,
    response: Response,
    current_user: User = Depends(get_current_user),
    jobdefs_name: str = Path(..., title="JobDef name to look for"),
    verbose: Optional[bareosBool] = Query("yes", title="Verbose output"),
):
    """
    Read all jobdef resources. Built on console command _show jobdefs_.

    Needs at least Bareos Version >= 20.0.0
    """
    return show_configuration_items(
        response=response,
        current_user=current_user,
        itemType="jobdefs",
        byName=jobdefs_name,
        verbose=verbose,
    )


#### Job Resource


@app.get("/configuration/jobs", tags=["jobs", "configuration"])
def read_all_jobs(
    *,
    response: Response,
    current_user: User = Depends(get_current_user),
    verbose: Optional[bareosBool] = Query("yes", title="Verbose output"),
):
    """
    Read all jobdef resources. Built on console command _show jobs_.

    Needs at least Bareos Version >= 20.0.0
    """
    return show_configuration_items(
        response=response, current_user=current_user, itemType="jobs", verbose=verbose
    )


@app.get("/configuration/jobs/{jobs_name}", tags=["jobs", "configuration"])
def read_job_by_name(
    *,
    response: Response,
    current_user: User = Depends(get_current_user),
    jobs_name: str = Path(..., title="Client name to look for"),
    verbose: Optional[bareosBool] = Query("yes", title="Verbose output"),
):
    """
    Read all jobdef resources. Built on console command _show jobs_.

    Needs at least Bareos Version >= 20.0.0
    """
    return show_configuration_items(
        response=response,
        current_user=current_user,
        itemType="jobs",
        byName=jobs_name,
        verbose=verbose,
    )


@app.post("/configuration/jobs", tags=["jobs", "configuration"])
def post_job(
    *,
    jobDef: jobResource = Body(..., title="Job resource"),
    response: Response,
    current_user: User = Depends(get_current_user),
):
    """
    Create a new job resource.
    Console command used: _configure add job_

    """
    return configure_add_standard_component(
        componentDef=jobDef,
        response=response,
        current_user=current_user,
        componentType="job",
    )


### Volumes


@app.get("/control/volumes", tags=["volumes", "control"])
def read_volumes(
    *,
    response: Response,
    current_user: User = Depends(get_current_user),
    limit: Optional[int] = Query(None, title="Result items limit", gt=0),
    offset: Optional[int] = Query(None, title="Result items offset", gt=0),
    myQuery: Optional[volumeQuery] = Body(None, title="Query parameter"),
):
    queryDict = {}
    countDict = {}
    responseDict = {}
    results = {}
    results["volumes"] = {}
    volumeKeyName = "volumes"
    volumeCommand = "llist volumes"
    countCommand = "list volumes"
    if myQuery is not None:
        queryDict = myQuery.dict()
    for q in queryDict:
        if queryDict[q] is not None:
            volumeCommand += " %s=%s" % (q, str(queryDict[q]))
            countCommand += " %s=%s" % (q, str(queryDict[q]))
    countCommand += " count"
    if limit is not None:
        volumeCommand += " limit=%d" % limit
    if offset is not None:
        volumeCommand += " offset=%d" % offset
    # if volume name is in queryDict, we have to remove the other filters
    # and use a different comman
    if "volume" in queryDict and queryDict["volume"] is not None:
        volumeKeyName = "volume"
        volumeCommand = "llist volume=%s" % queryDict["volume"]
        countCommand = None
    try:
        responseDict = current_user.jsonDirector.call(volumeCommand)
        if countCommand is not None:
            countDict = current_user.jsonDirector.call(countCommand)
    except Exception as e:
        response.status_code = 500
        return {
            "message": "Could not read volume list from director %s. Message: '%s'"
            % (CONFIG_DIRECTOR_NAME, e)
        }
    if "volumes" in responseDict:
        counter = 0
        # countDict/response dict has different structures, if filtered by pool or not
        if isinstance(countDict["volumes"], dict):
            results["volumes"] = responseDict[volumeKeyName]
            for p in countDict["volumes"]:
                counter += int(countDict["volumes"][p][0]["count"])
        else:
            # check for empty pool
            if len(responseDict[volumeKeyName]) == 0:
                response.status_code = 404
                return {"message": "Nothing found. Command: %s" % volumeCommand}
            poolName = responseDict[volumeKeyName][0]["pool"]
            # print(responseDict[volumeKeyName])
            results["volumes"][poolName] = responseDict[volumeKeyName]
            counter = int(countDict["volumes"][0]["count"])
        results["totalItems"] = counter
        foundItems = len(responseDict)
    elif "volume" in responseDict and "pool" in responseDict["volume"]:
        foundItems = 1
        poolName = responseDict["volume"]["pool"]
        results["totalItems"] = 1
        results["volumes"] = {poolName: [responseDict["volume"]]}
    else:
        response.status_code = 404
        return {"message": "nothing found for command: %s" % volumeCommand}

    if foundItems > 0:
        # check for limit / offset
        if limit is not None:
            results["limit"] = limit
        if offset is not None:
            results["offset"] = offset
        return results
    else:
        response.status_code = 404
        return {"message": "No volumes found."}


@app.get("/control/volumes/{volume_id}", tags=["volumes", "control"])
def read_volume(
    *,
    response: Response,
    current_user: User = Depends(get_current_user),
    volume_id: int = Path(..., title="Volume ID to look for", gt=0, example=1),
    volumeQuery: Optional[volumeQuery] = Body(None, title="Query parameter"),
):
    volumeCommand = "llist volumes"
    try:
        responseDict = current_user.jsonDirector.call(volumeCommand)
    except Exception as e:
        response.status_code = 500
        return {
            "message": "Could not read volume list from director %s. Message: '%s'"
            % (CONFIG_DIRECTOR_NAME, e)
        }
    if "volumes" in responseDict:
        for p in responseDict["volumes"]:
            for v in responseDict["volumes"][p]:
                if v["mediaid"] == str(volume_id):
                    response.status_code = 200
                    return v
    response.status_code = 404
    return {"message": "No volume with id %d found" % volume_id}


@app.post("/control/volumes", status_code=200, tags=["volumes", "control"])
def label_volume(
    *,
    response: Response,
    current_user: User = Depends(get_current_user),
    volumeLabel: volumeLabelDef = Body(..., title="Volume label properties"),
):
    """
    Label a new volume using the _label_" command
    """
    responseDict = {}
    labelCommand = "label"
    labelCommand += parseCommandOptions(volumeLabel.dict())
    try:
        responseDict = current_user.jsonDirector.call(labelCommand)
    except Exception as e:
        response.status_code = 500
        return {
            "message": "Could not label volume on director %s. Message: '%s'"
            % (CONFIG_DIRECTOR_NAME, e)
        }
    return responseDict


@app.patch("/control/volumes/{volume_name}", tags=["volumes", "control"])
def update_volume(
    *,
    response: Response,
    current_user: User = Depends(get_current_user),
    volume_name: str = Path(..., title="Volume Name to update", example="Full-1742"),
    volumeProps: volumeProperties = Body(..., title="Volume properties"),
):
    """
    Update a volume
    TODO: verify, that parameter are quoted correct
    """
    responseDict = {}
    updateCommand = "update volume=%s" % volume_name
    updateCommand += parseCommandOptions(volumeProps.dict())
    # print(updateCommand)
    try:
        responseDict = current_user.jsonDirector.call(updateCommand)
    except Exception as e:
        response.status_code = 500
        return {
            "message": "Could not update volume on director %s. Message: '%s'"
            % (CONFIG_DIRECTOR_NAME, e)
        }
    # Director delivers empty response, we want to return the changed volume's properties
    volQuery = volumeQuery()
    volQuery.volume = volume_name
    responseDict = read_volumes(
        response=response,
        current_user=current_user,
        myQuery=volQuery,
        limit=None,
        offset=None,
    )
    # TODO: responseDict is structured: {volumes:{poolname:[{volume}]}} - we just want to return the volume without list and pool dict around
    return responseDict


@app.put("/control/volumes/move", status_code=200, tags=["volumes", "control"])
def move_volume(
    *,
    response: Response,
    current_user: User = Depends(get_current_user),
    moveParams: volumeMove = Body(..., title="Volume move parameters"),
):
    """
    Move a volume, using the _move_ command
    TODO: handle encrypt flag
    """
    responseDict = {}
    updateCommand = "move"
    updateCommand += parseCommandOptions(moveParams.dict())
    # print (updateCommand)
    try:
        responseDict = current_user.jsonDirector.call(updateCommand)
    except Exception as e:
        response.status_code = 500
        return {
            "message": "Could not move volumes on director %s. Message: '%s'"
            % (CONFIG_DIRECTOR_NAME, e)
        }
    # Director delivers empty response
    return responseDict


@app.put("/control/volumes/export", status_code=200, tags=["volumes", "control"])
def export_volume(
    *,
    response: Response,
    current_user: User = Depends(get_current_user),
    exportParams: volumeExport = Body(..., title="Volume Export parameters"),
):
    """
    Export volumes the _export_ command
    """
    responseDict = {}
    updateCommand = "export"
    updateCommand += parseCommandOptions(exportParams.dict())
    # print(updateCommand)
    try:
        responseDict = current_user.jsonDirector.call(updateCommand)
    except Exception as e:
        response.status_code = 500
        return {
            "message": "Could not export volumes on director %s. Message: '%s'"
            % (CONFIG_DIRECTOR_NAME, e)
        }
    # Director delivers empty response
    response.status_code = 200
    return responseDict


@app.put("/control/volumes/import", status_code=200, tags=["volumes", "control"])
def import_volume(
    *,
    response: Response,
    current_user: User = Depends(get_current_user),
    importParams: volumeImport = Body(..., title="Volume import parameters"),
):
    """
    import volumes the _import_ command
    """
    responseDict = {}
    updateCommand = "import"
    updateCommand += parseCommandOptions(importParams.dict())
    # print(updateCommand)
    try:
        responseDict = current_user.jsonDirector.call(updateCommand)
    except Exception as e:
        response.status_code = 500
        return {
            "message": "Could not import volumes on director %s. Message: '%s'"
            % (CONFIG_DIRECTOR_NAME, e)
        }
    # Director delivers empty response
    response.status_code = 200
    return responseDict


@app.put("/control/volumes/{volume_name}", status_code=200, tags=["volumes", "control"])
def relabel_volume(
    *,
    response: Response,
    current_user: User = Depends(get_current_user),
    volume_name: str = Path(
        ..., title="Old Volume Name to relabel", example="Full-1742"
    ),
    volumeRelabel: volumeRelabelDef = Body(..., title="New label properties"),
):
    """
    Relabel a volume, using the _relabel_ command
    TODO: handle encrypt flag
    """
    responseDict = {}
    updateCommand = "relabel oldvolume=%s" % volume_name
    updateCommand += parseCommandOptions(volumeRelabel.dict())
    # print(updateCommand)
    try:
        responseDict = current_user.jsonDirector.call(updateCommand)
    except Exception as e:
        response.status_code = 500
        return {
            "message": "Could not relabel volume on director %s. Message: '%s'"
            % (CONFIG_DIRECTOR_NAME, e)
        }
    # Director delivers empty response
    response.status_code = 200
    return responseDict


@app.delete(
    "/control/volumes/{volume_name}", status_code=204, tags=["volumes", "control"]
)
def delete_volume(
    *,
    response: Response,
    current_user: User = Depends(get_current_user),
    volume_name: str = Path(..., title="Volume Name to delete", example="Full-1742"),
):
    """
    Delete a volume from catalog using the _delete volume_ command.
    """
    responseDict = {}
    deleteCommand = "delete volume=%s yes" % volume_name
    try:
        responseDict = current_user.jsonDirector.call(deleteCommand)
    except Exception as e:
        response.status_code = 500
        return {
            "message": "Could not delete volume on director %s. Message: '%s'"
            % (CONFIG_DIRECTOR_NAME, e)
        }
    # Director delivers empty response
    response.status_code = 204
    return responseDict


### Pools


@app.get("/configuration/pools", tags=["pools", "configuration"])
def read_all_pools(
    *,
    response: Response,
    current_user: User = Depends(get_current_user),
    verbose: Optional[bareosBool] = Query("yes", title="Verbose output"),
):
    """
    Read all jobdef resources. Built on console command _show pools_.

    Needs at least Bareos Version >= 20.0.0
    """
    return show_configuration_items(
        response=response, current_user=current_user, itemType="pools", verbose=verbose
    )


@app.get("/configuration/pools/{pools_name}", tags=["pools", "configuration"])
def read_pool_by_name(
    *,
    response: Response,
    current_user: User = Depends(get_current_user),
    pools_name: str = Path(..., title="Client name to look for"),
    verbose: Optional[bareosBool] = Query("yes", title="Verbose output"),
):
    """
    Read all jobdef resources. Built on console command _show pools_.

    Needs at least Bareos Version >= 20.0.0
    """
    return show_configuration_items(
        response=response,
        current_user=current_user,
        itemType="pools",
        byName=pools_name,
        verbose=verbose,
    )


@app.post("/configuration/pools", tags=["pools", "configuration"])
def post_pool(
    *,
    poolDef: poolResource = Body(..., title="pool resource"),
    response: Response,
    current_user: User = Depends(get_current_user),
):
    """
    Create a new pool resource.
    Console command used: _configure add pool_
    """
    return configure_add_standard_component(
        componentDef=poolDef,
        response=response,
        current_user=current_user,
        componentType="pool",
    )


@app.get("/control/pools", status_code=200, tags=["pools", "control"])
def read_all_pools(
    response: Response,
    current_user: User = Depends(get_current_user),
    name: Optional[str] = None,
):
    """
    Read settings for all pools or just one pool by name from catalog.
    Built on console command _llist pool_
    """
    listCommand = ""
    if name:
        listCommand = "llist pool=%s" % name
    else:
        listCommand = "llist pools"
    # print(listCommand)
    try:
        responseDict = current_user.jsonDirector.call(listCommand)
    except Exception as e:
        response.status_code = 500
        return {
            "message": "Could not read pool list from director %s. Message: '%s'"
            % (CONFIG_DIRECTOR_NAME, e)
        }
    if "pools" in responseDict:
        totalItems = len(responseDict["pools"])
        return {"totalItems": totalItems, "pools": responseDict["pools"]}
    else:
        response.status_code = 404
        return {"message": "No pools found."}


@app.get("/control/pools/{pool_id}", tags=["pools", "control"])
def read_pool(
    *,
    pool_id: int = Path(..., title="The ID of pool to get", ge=1),
    response: Response,
    current_user: User = Depends(get_current_user),
):
    """
    Read catalog information abour just one pool by id.
    Built on console command _llist pool_

    **Warning** Director does not support direct query by _id_ we query all pools and filter the result.
    Maybe more time consuming than expected in large settings.
    """
    allpools = read_all_pools(response, current_user)
    result = None
    for c in allpools["pools"]:
        if c["poolid"] == str(pool_id):
            result = c
            break
    if result:
        return result
    else:
        response.status_code = 404
        return {
            "message": "pool with pool ID {pool_id} not found".format(pool_id=pool_id)
        }
    return {"item_id": item_id}


### Schedules


@app.get("/configuration/schedules", tags=["schedules", "configuration"])
def read_all_schedules(
    *,
    response: Response,
    current_user: User = Depends(get_current_user),
    verbose: Optional[bareosBool] = Query("yes", title="Verbose output"),
):
    """
    Read all jobdef resources. Built on console command _show schedules_.

    Needs at least Bareos Version >= 20.0.0
    """
    return show_configuration_items(
        response=response,
        current_user=current_user,
        itemType="schedules",
        verbose=verbose,
    )


@app.get(
    "/configuration/schedules/{schedules_name}", tags=["schedules", "configuration"]
)
def read_schedule_by_name(
    *,
    response: Response,
    current_user: User = Depends(get_current_user),
    schedules_name: str = Path(..., title="Client name to look for"),
    verbose: Optional[bareosBool] = Query("yes", title="Verbose output"),
):
    """
    Read all jobdef resources. Built on console command _show schedules_.

    Needs at least Bareos Version >= 20.0.0
    """
    return show_configuration_items(
        response=response,
        current_user=current_user,
        itemType="schedules",
        byName=schedules_name,
        verbose=verbose,
    )


@app.post("/configuration/schedules", tags=["schedules", "configuration"])
def create_schedule(
    *,
    response: Response,
    current_user: User = Depends(get_current_user),
    scheduleDef: scheduleResource = Body(..., title="Name for new schedule"),
):
    """
    Create a new schedule resource.
    Console command used _configure add schedule_
    """
    return configure_add_standard_component(
        response=response,
        componentDef=scheduleDef,
        componentType="schedule",
        current_user=current_user,
    )


@app.put(
    "/control/schedules/enable/{schedule_name}",
    status_code=204,
    tags=["schedules", "control"],
)
def enable_schedule(
    *,
    schedule_name: str = Path(..., title="The schedule (name) to enable"),
    response: Response,
    current_user: User = Depends(get_current_user),
):
    (result, jsonMessage) = switch_resource(
        response=response,
        current_user=current_user,
        resourceName=schedule_name,
        componentType="schedule",
        enable=True,
    )
    return jsonMessage


@app.put(
    "/control/schedules/disable/{schedule_name}",
    status_code=204,
    tags=["schedules", "control"],
)
def disable_schedule(
    *,
    schedule_name: str = Path(..., title="The schedule (name) to disable"),
    response: Response,
    current_user: User = Depends(get_current_user),
):
    (result, jsonMessage) = switch_resource(
        response=response,
        current_user=current_user,
        resourceName=schedule_name,
        componentType="schedule",
        enable=False,
    )
    return jsonMessage


### Storages


@app.get("/configuration/storages", tags=["storages", "configuration"])
def read_all_storages(
    *,
    response: Response,
    current_user: User = Depends(get_current_user),
    verbose: Optional[bareosBool] = Query("yes", title="Verbose output"),
):
    """
    Read all jobdef resources. Built on console command _show storages_.

    Needs at least Bareos Version >= 20.0.0
    """
    return show_configuration_items(
        response=response,
        current_user=current_user,
        itemType="storages",
        verbose=verbose,
    )


@app.get("/configuration/storages/{storages_name}", tags=["storages", "configuration"])
def read_storage_by_name(
    *,
    response: Response,
    current_user: User = Depends(get_current_user),
    storages_name: str = Path(..., title="Client name to look for"),
    verbose: Optional[bareosBool] = Query("yes", title="Verbose output"),
):
    """
    Read all jobdef resources. Built on console command _show storages_.

    Needs at least Bareos Version >= 20.0.0
    """
    return show_configuration_items(
        response=response,
        current_user=current_user,
        itemType="storages",
        byName=storages_name,
        verbose=verbose,
    )


@app.post("/configuration/storage", tags=["storages", "configuration"])
def post_storage(
    *,
    storageDef: storageResource = Body(..., title="storage resource"),
    response: Response,
    current_user: User = Depends(get_current_user),
):
    """
    Create a new storage resource.
    Console command used: _configure add storage_
    """
    return configure_add_standard_component(
        response=response,
        componentDef=storageDef,
        componentType="storage",
        current_user=current_user,
    )


### Users, profiles, consoles


@app.get("/configuration/users", tags=["users", "configuration"])
def read_all_users(
    *,
    response: Response,
    current_user: User = Depends(get_current_user),
    verbose: Optional[bareosBool] = Query("yes", title="Verbose output"),
):
    """
    Read all users resources. Built on console command _show users_.

    Needs at least Bareos Version >= 20.0.0
    """
    return show_configuration_items(
        response=response, current_user=current_user, itemType="users", verbose=verbose
    )


@app.get("/configuration/users/{users_name}", tags=["users", "configuration"])
def read_user_by_name(
    *,
    response: Response,
    current_user: User = Depends(get_current_user),
    users_name: str = Path(..., title="Client name to look for"),
    verbose: Optional[bareosBool] = Query("yes", title="Verbose output"),
):
    """
    Read all jobdef resources. Built on console command _show users_.

    Needs at least Bareos Version >= 20.0.0
    """
    return show_configuration_items(
        response=response,
        current_user=current_user,
        itemType="users",
        byName=users_name,
        verbose=verbose,
    )


@app.post("/configuration/users", tags=["users", "configuration"])
def post_user(
    *,
    userDef: userResource = Body(..., title="user resource"),
    response: Response,
    current_user: User = Depends(get_current_user),
):
    """
    Create a new Bareos user resource.
    Console command used: _configure add user_

    """
    return configure_add_standard_component(
        response=response,
        componentDef=userDef,
        componentType="user",
        current_user=current_user,
    )


@app.get("/configuration/profiles", tags=["profiles", "configuration"])
def read_all_profiles(
    *,
    response: Response,
    current_user: User = Depends(get_current_user),
    verbose: Optional[bareosBool] = Query("yes", title="Verbose output"),
):
    """
    Read all jobdef resources. Built on console command _show profiles_.

    Needs at least Bareos Version >= 20.0.0
    """
    return show_configuration_items(
        response=response,
        current_user=current_user,
        itemType="profiles",
        verbose=verbose,
    )


@app.get("/configuration/profiles/{profiles_name}", tags=["profiles", "configuration"])
def read_client_by_name(
    *,
    response: Response,
    current_user: User = Depends(get_current_user),
    profiles_name: str = Path(..., title="Client name to look for"),
    verbose: Optional[bareosBool] = Query("yes", title="Verbose output"),
):
    """
    Read all jobdef resources. Built on console command _show profiles_.

    Needs at least Bareos Version >= 20.0.0
    """
    return show_configuration_items(
        response=response,
        current_user=current_user,
        itemType="profiles",
        byName=profiles_name,
        verbose=verbose,
    )


@app.post("/configuration/profiles", tags=["users", "configuration"])
def post_profile(
    *,
    profileDef: profileResource = Body(..., title="profile resource"),
    response: Response,
    current_user: User = Depends(get_current_user),
):
    """
    Create a new Bareos profile resource.
    Console command used: _configure add profile_

    """
    return configure_add_standard_component(
        response=response,
        componentDef=profileDef,
        componentType="profile",
        current_user=current_user,
    )


@app.get("/configuration/consoles", tags=["consoles", "configuration"])
def read_all_consoles(
    *,
    response: Response,
    current_user: User = Depends(get_current_user),
    verbose: Optional[bareosBool] = Query("yes", title="Verbose output"),
):
    """
    Read all jobdef resources. Built on console command _show consoles_.

    Needs at least Bareos Version >= 20.0.0
    """
    return show_configuration_items(
        response=response,
        current_user=current_user,
        itemType="consoles",
        verbose=verbose,
    )


@app.get("/configuration/consoles/{consoles_name}", tags=["consoles", "configuration"])
def read_console_by_name(
    *,
    response: Response,
    current_user: User = Depends(get_current_user),
    consoles_name: str = Path(..., title="Console name to look for"),
    verbose: Optional[bareosBool] = Query("yes", title="Verbose output"),
):
    """
    Read all jobdef resources. Built on console command _show consoles_.

    Needs at least Bareos Version >= 20.0.0
    """
    return show_configuration_items(
        response=response,
        current_user=current_user,
        itemType="consoles",
        byName=consoles_name,
        verbose=verbose,
    )


@app.post("/configuration/consoles", tags=["users", "configuration"])
def post_console(
    *,
    consoleDef: consoleResource = Body(..., title="console resource"),
    response: Response,
    current_user: User = Depends(get_current_user),
):
    """
    Create a new Bareos console resource.
    Console command used: _configure add console_

    """
    return configure_add_standard_component(
        response=response,
        componentDef=consoleDef,
        componentType="console",
        current_user=current_user,
    )


### Director


@app.get("/control/directors/version", tags=["directors", "control"])
def read_director_version(
    *, response: Response, current_user: User = Depends(get_current_user)
):
    """
    Read director version. Command used: _version_
    """
    result = None
    dirCommand = "version"
    try:
        result = current_user.jsonDirector.call(dirCommand)
    except Exception as e:
        raise HTTPException(
            status_code=500, detail="Could not read version from director"
        )
    if result and "version" in result:
        return result["version"]
    else:
        response.status_code = 404
        return {"message": "No version info returned"}


@app.get("/control/directors/time", tags=["directors", "control"])
def read_director_time(
    *, response: Response, current_user: User = Depends(get_current_user)
):
    """
    Read director time. Command used: _time_
    """
    result = None
    dirCommand = "time"
    try:
        result = current_user.jsonDirector.call(dirCommand)
    except Exception as e:
        response.status_code = 500
        return {
            "message": "Could not read director time %s. Message: '%s'"
            % (current_user.directorName, e)
        }
    if result and "time" in result:
        return result["time"]
    else:
        response.status_code = 404
        return {"message": "No time info returned"}


@app.put("/control/directors/reload", tags=["directors", "control"])
def read_director_time(
    *, response: Response, current_user: User = Depends(get_current_user)
):
    """
    Reload director configuration from files. Command used: _reload_
    """
    result = None
    dirCommand = "reload"
    try:
        result = current_user.jsonDirector.call(dirCommand)
    except Exception as e:
        response.status_code = 500
        return {
            "message": "Could not reload director %s. Message: '%s'"
            % (current_user.directorName, e)
        }
    if result and "reload" in result:
        return result["reload"]
    else:
        response.status_code = 500
        return result
