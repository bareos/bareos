#!/usr/bin/env python

import yaml
import lxml.etree as ET

# create the XML file structure
project = ET.Element('project')
title = ET.SubElement(project, 'title')
description = ET.SubElement(project, 'description')
build = ET.SubElement(project, 'build')
disable = ET.SubElement(build, 'disable')
disable.set('repository','win_cross')





with open("../matrix.yml", 'r') as stream:
    try:
        data = yaml.load(stream)
        for OS in data['OS']:
            #print "OS:", OS #, " -> ",  data["OS"][OS], "\n\n"
            for VERSION in data['OS'][OS]:
                repository = ET.SubElement(project,'repository')
                repository.set('name', OS + "_" + str(VERSION))

                #print "   ", VERSION
                #for  PROJECTPATH in data['OS'][OS][VERSION]['PROJECTPATH']:
                #    print "       ", PROJECTPATH
                for  PROJECTPATH in data['OS'][OS][VERSION]['PROJECTPATH']:
                    path = ET.SubElement(repository, 'path')
                    path.set('project', PROJECTPATH['PATH'])
                    path.set('repository',  PROJECTPATH['REPOSITORY']
)
                    #print "      PATH, REPO :", PROJECTPATH['PATH'], PROJECTPATH['REPOSITORY']
                for  ARCH in data['OS'][OS][VERSION]['ARCH']:
                    arch = ET.SubElement(repository, 'arch')
                    arch.text =ARCH
                    #print "      ARCH: ", ARCH

    except yaml.YAMLError as exc:
        print(exc)

print ( ET.tostring(project, pretty_print=True))
