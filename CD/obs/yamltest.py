#!/usr/bin/env python

import yaml

with open("../matrix.yml", 'r') as stream:
    try:
        data = yaml.load(stream)
        for OS in data['OS']:
            print "OS:", OS #, " -> ",  data["OS"][OS], "\n\n"
            for VERSION in data['OS'][OS]:
                print "   ", VERSION
                for  ARCH in data['OS'][OS][VERSION]['ARCH']:
                    print "      ARCH: ", ARCH
                #for  PROJECTPATH in data['OS'][OS][VERSION]['PROJECTPATH']:
                #    print "       ", PROJECTPATH
                for  PROJECTPATH in data['OS'][OS][VERSION]['PROJECTPATH']:
                    print "      PATH, REPO :", PROJECTPATH['PATH'], PROJECTPATH['REPOSITORY']

        #print (data['OS']['Fedora'])
        #print (yaml.dump(data))
    except yaml.YAMLError as exc:
        print(exc)
