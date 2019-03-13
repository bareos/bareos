#!/usr/bin/env python

import yaml
import lxml.etree as ET

axis = ET.Element('hudson.matrix.TextAxis')
name = ET.SubElement(axis, 'name')
name.text = "DISTRELEASE"
values = ET.SubElement(axis, 'values')



with open("../matrix.yml", 'r') as stream:
    try:
        data = yaml.load(stream)
        for OS in data['OS']:
            for VERSION in data['OS'][OS]:
                for  ARCH in data['OS'][OS][VERSION]['ARCH']:
                    distrelease = ET.SubElement(values, 'string')
                    distrelease.text = OS + "-" + str(VERSION) + '-' + ARCH

    except yaml.YAMLError as exc:
        print(exc)

#print ( ET.tostring(axis, pretty_print=True))
print ( ET.tostring(axis))
